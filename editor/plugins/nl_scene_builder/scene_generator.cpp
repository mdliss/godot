/**************************************************************************/
/*  scene_generator.cpp                                                  */
/**************************************************************************/

#include "scene_generator.h"

#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/variant/callable.h"
#include "core/object/class_db.h"
#include "core/string/node_path.h"
#include "core/variant/variant.h"
#include "scene/main/node.h"

void SceneGenerator::set_editor_interface(EditorInterface *p_interface) {
	editor_interface = p_interface;
}

void SceneGenerator::set_undo_redo(EditorUndoRedoManager *p_undo_redo) {
	undo_redo = p_undo_redo;
}

Node *SceneGenerator::_create_node(const NodeDefinition &p_def, Node *p_parent) {
	Node *fallback_parent = p_parent ? p_parent : scene_owner;
	if (!fallback_parent) {
		return nullptr;
	}

	Node *resolved_parent = _resolve_parent(p_def, fallback_parent);
	if (!resolved_parent) {
		warnings.push_back("No valid parent found for node: " + p_def.name);
		return nullptr;
	}

	if (!ClassDB::class_exists(p_def.type)) {
		warnings.push_back("Unknown node type: " + p_def.type);
		return nullptr;
	}

	Object *obj = ClassDB::instantiate(p_def.type);
	Node *node = Object::cast_to<Node>(obj);
	if (!node) {
		memdelete(obj);
		warnings.push_back("Failed to instantiate node type: " + p_def.type);
		return nullptr;
	}

	node->set_name(p_def.name);
	resolved_parent->add_child(node);

	// Set owner so node appears in editor scene tree
	if (scene_owner) {
		node->set_owner(scene_owner);
	}

	_apply_properties(node, p_def.properties);

	for (int i = 0; i < p_def.children.size(); i++) {
		_create_node(p_def.children[i], node);
	}

	return node;
}

Node *SceneGenerator::_resolve_parent(const NodeDefinition &p_def, Node *p_fallback_parent) {
	if (!scene_owner) {
		return p_fallback_parent;
	}

	if (p_def.parent.is_empty()) {
		return p_fallback_parent;
	}

	Node *parent = nullptr;
	NodePath parent_path(p_def.parent);
	if (scene_owner->has_node(parent_path)) {
		parent = scene_owner->get_node(parent_path);
	}

	if (!parent) {
		parent = _find_node_by_name(scene_owner, p_def.parent);
	}

	if (!parent) {
		warnings.push_back("Parent '" + p_def.parent + "' not found; using fallback parent.");
		return p_fallback_parent;
	}

	return parent;
}

bool SceneGenerator::_has_available_parent(const NodeDefinition &p_def) {
	if (p_def.parent.is_empty()) {
		return true;
	}

	if (!scene_owner) {
		return false;
	}

	NodePath parent_path(p_def.parent);
	if (scene_owner->has_node(parent_path)) {
		return true;
	}

	return _find_node_by_name(scene_owner, p_def.parent) != nullptr;
}

Ref<Resource> SceneGenerator::_create_resource(const Dictionary &p_dict) {
	String path;
	if (p_dict.has("resource_path")) {
		path = p_dict["resource_path"];
	} else if (p_dict.has("path")) {
		path = p_dict["path"];
	} else if (p_dict.has("load")) {
		path = p_dict["load"];
	}

	if (!path.is_empty()) {
		Ref<Resource> loaded = ResourceLoader::load(path);
		if (loaded.is_valid()) {
			return loaded;
		}
	}

	if (!p_dict.has("_type")) {
		return Ref<Resource>();
	}

	String type_name = p_dict["_type"];
	if (!ClassDB::class_exists(type_name)) {
		return Ref<Resource>();
	}

	Object *obj = ClassDB::instantiate(type_name);
	Resource *res = Object::cast_to<Resource>(obj);
	if (!res) {
		if (obj) {
			memdelete(obj);
		}
		return Ref<Resource>();
	}

	Ref<Resource> ref_res = Ref<Resource>(res);

	// Apply properties to the resource
	LocalVector<Variant> keys = p_dict.get_key_list();
	for (const Variant &key : keys) {
		String key_str = key;
		if (key_str == "_type") {
			continue;
		}

		Variant value = p_dict[key];

		// Handle Vector2 represented as {x, y}
		if (value.get_type() == Variant::DICTIONARY) {
			Dictionary val_dict = value;
			if (val_dict.has("x") && val_dict.has("y") && val_dict.size() == 2) {
				float x_val = val_dict["x"];
				float y_val = val_dict["y"];
				Vector2 vec(x_val, y_val);
				res->set(key_str, vec);
				continue;
			}
		}

		res->set(key_str, value);
	}

	return ref_res;
}

void SceneGenerator::_apply_properties(Node *p_node, const Dictionary &p_properties) {
	LocalVector<Variant> keys = p_properties.get_key_list();
	for (const Variant &key : keys) {
		StringName prop_name = key;
		Variant value = p_properties[key];

		// Handle nested resource objects (like shapes)
		if (value.get_type() == Variant::DICTIONARY) {
			Dictionary val_dict = value;
			if (val_dict.has("_type")) {
				Ref<Resource> res = _create_resource(val_dict);
				if (res.is_valid()) {
					p_node->set(prop_name, res);
					continue;
				}
			}
			if (val_dict.has("resource_path") || val_dict.has("path") || val_dict.has("load")) {
				Ref<Resource> res = _create_resource(val_dict);
				if (res.is_valid()) {
					p_node->set(prop_name, res);
					continue;
				}
			}
			// Handle Vector2 represented as {x, y}
			if (val_dict.has("x") && val_dict.has("y") && val_dict.size() == 2) {
				float x_val = val_dict["x"];
				float y_val = val_dict["y"];
				Vector2 vec(x_val, y_val);
				p_node->set(prop_name, vec);
				continue;
			}
			// Handle Color represented as {r, g, b, a}
			if (val_dict.has("r") && val_dict.has("g") && val_dict.has("b")) {
				float r = val_dict["r"];
				float g = val_dict["g"];
				float b = val_dict["b"];
				float a = val_dict.has("a") ? float(val_dict["a"]) : 1.0f;
				Color col(r, g, b, a);
				p_node->set(prop_name, col);
				continue;
			}
		}

		if (value.get_type() == Variant::STRING) {
			String path = value;
			if ((path.begins_with("res://") || path.begins_with("user://")) && ResourceLoader::exists(path)) {
				Ref<Resource> res = ResourceLoader::load(path);
				if (res.is_valid()) {
					p_node->set(prop_name, res);
					continue;
				}
			}
		}

		// Handle arrays of Vector2 (for Polygon2D.polygon, Line2D.points)
		if (value.get_type() == Variant::ARRAY) {
			Array arr = value;
			if (arr.size() > 0) {
				Variant first = arr[0];
				// Check if it's an array of [x, y] pairs
				if (first.get_type() == Variant::ARRAY) {
					PackedVector2Array vec_arr;
					for (int i = 0; i < arr.size(); i++) {
						Array point = arr[i];
						if (point.size() >= 2) {
							float x = point[0];
							float y = point[1];
							vec_arr.push_back(Vector2(x, y));
						}
					}
					p_node->set(prop_name, vec_arr);
					continue;
				}
				// Check if it's an array of {x, y} dictionaries
				if (first.get_type() == Variant::DICTIONARY) {
					Dictionary first_dict = first;
					if (first_dict.has("x") && first_dict.has("y")) {
						PackedVector2Array vec_arr;
						for (int i = 0; i < arr.size(); i++) {
							Dictionary point = arr[i];
							float x = point["x"];
							float y = point["y"];
							vec_arr.push_back(Vector2(x, y));
						}
						p_node->set(prop_name, vec_arr);
						continue;
					}
				}
			}
		}

		p_node->set(prop_name, value);
	}
}

Node *SceneGenerator::_find_node_by_name(Node *p_root, const String &p_name) {
	if (p_root->get_name() == p_name) {
		return p_root;
	}
	for (int i = 0; i < p_root->get_child_count(); i++) {
		Node *found = _find_node_by_name(p_root->get_child(i), p_name);
		if (found) {
			return found;
		}
	}
	return nullptr;
}

void SceneGenerator::_attach_script(Node *p_node, const String &p_code) {
	if (!p_node || p_code.is_empty()) {
		return;
	}

	// Generate a unique script path
	String node_name = p_node->get_name();
	String base_path = "res://" + node_name.to_lower() + "_script";
	String script_path = base_path + ".gd";

	// Find unique filename if exists
	int counter = 1;
	while (FileAccess::exists(script_path)) {
		script_path = base_path + "_" + itos(counter) + ".gd";
		counter++;
	}

	Ref<GDScript> script;
	script.instantiate();
	script->set_source_code(p_code);
	script->set_path(script_path);

	// Save the script to file
	Error save_err = ResourceSaver::save(script, script_path);
	if (save_err != OK) {
		warnings.push_back("Failed to save script for node: " + node_name);
		return;
	}

	// Reload to compile
	Error err = script->reload();
	if (err != OK) {
		warnings.push_back("Failed to compile script for node: " + node_name);
		return;
	}

	p_node->set_script(script);
}

void SceneGenerator::_connect_signals(const ParseResult &p_parsed, Node *p_scene_root) {
	for (const SignalConnection &sig : p_parsed.signals) {
		if (sig.source_node.is_empty() || sig.signal_name.is_empty()) {
			continue;
		}

		Node *source = _find_node_by_name(p_scene_root, sig.source_node);
		if (!source) {
			warnings.push_back("Signal source not found: " + sig.source_node);
			continue;
		}

		Node *target = sig.target_node.is_empty() ? source : _find_node_by_name(p_scene_root, sig.target_node);
		if (!target) {
			warnings.push_back("Signal target not found: " + sig.target_node);
			continue;
		}

		if (!source->has_signal(sig.signal_name)) {
			warnings.push_back("Signal '" + sig.signal_name + "' does not exist on " + sig.source_node);
			continue;
		}

		if (sig.method_name.is_empty()) {
			warnings.push_back("Signal '" + sig.signal_name + "' missing method binding.");
			continue;
		}

		Callable callable(target, StringName(sig.method_name));
		if (!callable.is_valid()) {
			warnings.push_back("Invalid callable for signal '" + sig.signal_name + "' -> " + sig.method_name);
			continue;
		}

		if (source->is_connected(sig.signal_name, callable)) {
			continue;
		}

		Error err = source->connect(sig.signal_name, callable);
		if (err != OK) {
			warnings.push_back("Failed to connect signal '" + sig.signal_name + "': " + itos(err));
		}
	}
}

SceneGenerator::GenerationResult SceneGenerator::generate(const ParseResult &p_parsed, Node *p_scene_root) {
	GenerationResult result;
	result.success = false;
	result.nodes_created = 0;
	warnings.clear();

	if (!p_scene_root) {
		result.error_message = "No scene root available for generation.";
		return result;
	}

	// Store scene root for ownership assignment
	scene_owner = p_scene_root;

	// Create all nodes first, ensuring declared parents exist before instancing children.
	Vector<NodeDefinition> pending = p_parsed.nodes;
	while (!pending.is_empty()) {
		Vector<NodeDefinition> next_pending;
		bool progress = false;
		for (int i = 0; i < pending.size(); i++) {
			const NodeDefinition &def = pending[i];
			if (_has_available_parent(def)) {
				Node *created = _create_node(def, p_scene_root);
				if (created) {
					result.nodes_created++;
				}
				progress = true;
			} else {
				next_pending.push_back(def);
			}
		}

		if (!progress) {
			for (int i = 0; i < next_pending.size(); i++) {
				const NodeDefinition &def = next_pending[i];
				warnings.push_back("Parent '" + def.parent + "' not found for node '" + def.name + "'. Placing under scene root.");
				Node *created = _create_node(def, p_scene_root);
				if (created) {
					result.nodes_created++;
				}
			}
			break;
		}

		pending = next_pending;
	}

	// Then attach scripts
	if (p_parsed.scripts.size() == 0) {
		warnings.push_back("No scripts in LLM response - movement won't work");
	}
	for (const ScriptDefinition &script_def : p_parsed.scripts) {
		Node *target = _find_node_by_name(p_scene_root, script_def.attach_to);
		if (target) {
			_attach_script(target, script_def.code);
			warnings.push_back("Attached script to: " + script_def.attach_to);
		} else {
			warnings.push_back("Could not find node to attach script: " + script_def.attach_to);
		}
	}

	_connect_signals(p_parsed, p_scene_root);

	result.success = true;
	result.warnings = warnings;
	return result;
}
