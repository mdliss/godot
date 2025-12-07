/**************************************************************************/
/*  response_parser.cpp                                                  */
/**************************************************************************/

#include "response_parser.h"

#include "core/io/json.h"
#include "core/object/class_db.h"

void ResponseParser::_bind_methods() {
	// No public methods exposed to scripting for now.
}

bool ResponseParser::_validate_node_type(const String &p_type) const {
	if (p_type.is_empty()) {
		return false;
	}
	return ClassDB::class_exists(p_type);
}

NodeDefinition ResponseParser::_parse_node(const Dictionary &p_node_dict) const {
	NodeDefinition def;
	if (p_node_dict.has("name")) {
		def.name = p_node_dict["name"];
	}
	if (p_node_dict.has("type")) {
		def.type = p_node_dict["type"];
	}
	if (p_node_dict.has("parent")) {
		def.parent = p_node_dict["parent"];
	}
	if (p_node_dict.has("properties")) {
		def.properties = p_node_dict["properties"];
	}
	if (p_node_dict.has("children")) {
		Array children = p_node_dict["children"];
		def.children.resize(children.size());
		for (int i = 0; i < children.size(); i++) {
			Dictionary child_dict = children[i];
			def.children.write[i] = _parse_node(child_dict);
		}
	}
	return def;
}

ScriptDefinition ResponseParser::_parse_script(const Dictionary &p_script_dict) const {
	ScriptDefinition def;
	if (p_script_dict.has("attach_to")) {
		def.attach_to = p_script_dict["attach_to"];
	}
	if (p_script_dict.has("code")) {
		def.code = p_script_dict["code"];
	}
	return def;
}

SignalConnection ResponseParser::_parse_signal(const Dictionary &p_signal_dict) const {
	SignalConnection con;
	if (p_signal_dict.has("source")) {
		con.source_node = p_signal_dict["source"];
	}
	if (p_signal_dict.has("signal")) {
		con.signal_name = p_signal_dict["signal"];
	}
	if (p_signal_dict.has("target")) {
		con.target_node = p_signal_dict["target"];
	}
	if (p_signal_dict.has("method")) {
		con.method_name = p_signal_dict["method"];
	}
	return con;
}

String ResponseParser::_extract_json(const String &p_response) const {
	String text = p_response.strip_edges();

	// Try to extract JSON from markdown code blocks: ```json ... ``` or ``` ... ```
	int json_start = text.find("```json");
	if (json_start != -1) {
		json_start += 7; // Skip past ```json
		int json_end = text.find("```", json_start);
		if (json_end != -1) {
			return text.substr(json_start, json_end - json_start).strip_edges();
		}
	}

	// Try generic code block
	int code_start = text.find("```");
	if (code_start != -1) {
		code_start += 3;
		// Skip language identifier if on same line
		int newline = text.find("\n", code_start);
		if (newline != -1 && newline - code_start < 20) {
			code_start = newline + 1;
		}
		int code_end = text.find("```", code_start);
		if (code_end != -1) {
			return text.substr(code_start, code_end - code_start).strip_edges();
		}
	}

	// Look for JSON object directly
	int brace_start = text.find("{");
	if (brace_start != -1) {
		int brace_end = text.rfind("}");
		if (brace_end > brace_start) {
			return text.substr(brace_start, brace_end - brace_start + 1);
		}
	}

	return text;
}

ParseResult ResponseParser::parse(const String &p_json_string) const {
	ParseResult result;

	String json_text = _extract_json(p_json_string);

	Ref<JSON> json_parser = memnew(JSON);
	Error err = json_parser->parse(json_text);
	if (err != OK) {
		result.success = false;
		result.error_message = "Failed to parse JSON response. Raw: " + p_json_string.left(500);
		return result;
	}

	Variant root_var = json_parser->get_data();
	if (root_var.get_type() != Variant::DICTIONARY) {
		result.success = false;
		result.error_message = "Top-level JSON must be an object.";
		return result;
	}

	Dictionary root = root_var;

	// Nodes
	if (root.has("nodes")) {
		Array nodes = root["nodes"];
		for (int i = 0; i < nodes.size(); i++) {
			Dictionary node_dict = nodes[i];
			NodeDefinition def = _parse_node(node_dict);
			if (!_validate_node_type(def.type)) {
				// Skip unknown types but continue.
				continue;
			}
			result.nodes.push_back(def);
		}
	}

	// Scripts
	if (root.has("scripts")) {
		Array scripts = root["scripts"];
		for (int i = 0; i < scripts.size(); i++) {
			Dictionary script_dict = scripts[i];
			result.scripts.push_back(_parse_script(script_dict));
		}
	}

	// Signals
	if (root.has("signals")) {
		Array signals = root["signals"];
		for (int i = 0; i < signals.size(); i++) {
			Dictionary signal_dict = signals[i];
			result.signals.push_back(_parse_signal(signal_dict));
		}
	}

	result.success = true;
	return result;
}

