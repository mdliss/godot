/* SceneGenerator - create Godot nodes from parsed LLM output.
 *
 * This MVP version focuses on creating simple nodes under the current scene
 * root using NodeDefinition / ScriptDefinition / SignalConnection. It is
 * designed to be driven from NLInputPanel in later work.
 */

#pragma once

#include "core/io/resource.h"
#include "core/object/ref_counted.h"
#include "modules/gdscript/gdscript.h"

#include "editor/editor_interface.h"
#include "editor/editor_undo_redo_manager.h"

#include "nl_types.h"

class SceneGenerator : public RefCounted {
	GDCLASS(SceneGenerator, RefCounted);

	EditorInterface *editor_interface = nullptr;
	EditorUndoRedoManager *undo_redo = nullptr;

	Vector<String> warnings;
	Node *scene_owner = nullptr;
	String generation_mode = "full";

	Node *_create_node(const NodeDefinition &p_def, Node *p_parent);
	Node *_find_node_by_name(Node *p_root, const String &p_name);
	Node *_resolve_parent(const NodeDefinition &p_def, Node *p_fallback_parent);
	bool _has_available_parent(const NodeDefinition &p_def);
	Ref<Resource> _create_resource(const Dictionary &p_dict);
	void _apply_properties(Node *p_node, const Dictionary &p_properties);
	void _attach_script(Node *p_node, const String &p_code);
	void _connect_signals(const ParseResult &p_parsed, Node *p_scene_root);

public:
	struct GenerationResult {
		bool success = false;
		int nodes_created = 0;
		Vector<String> warnings;
		String error_message;
	};

	void set_editor_interface(EditorInterface *p_interface);
	void set_undo_redo(EditorUndoRedoManager *p_undo_redo);
	void set_generation_mode(const String &p_mode);

	GenerationResult generate(const ParseResult &p_parsed, Node *p_scene_root);
};
