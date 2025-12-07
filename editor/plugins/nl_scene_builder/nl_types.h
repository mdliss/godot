/* Shared type definitions for the NL Scene Builder plugin.
 *
 * These mirror the JSON schema produced by the LLM and consumed by the
 * SceneGenerator. They are kept minimal for the MVP.
 */

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"

struct NodeDefinition {
	String name;
	String type;
	String parent;
	Dictionary properties;
	Vector<NodeDefinition> children;
};

struct ScriptDefinition {
	String attach_to;
	String code;
};

struct SignalConnection {
	String source_node;
	String signal_name;
	String target_node;
	String method_name;
};

struct ParseResult {
	bool success = false;
	String error_message;
	Vector<NodeDefinition> nodes;
	Vector<ScriptDefinition> scripts;
	Vector<SignalConnection> signals;
};

