/* ResponseParser - Parse and validate LLM JSON responses.
 *
 * This MVP implementation focuses on turning a JSON string into the
 * NodeDefinition / ScriptDefinition / SignalConnection structs from
 * nl_types.h, with basic validation.
 */

#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"

#include "nl_types.h"

class ResponseParser : public RefCounted {
	GDCLASS(ResponseParser, RefCounted);

protected:
	static void _bind_methods();

	String _extract_json(const String &p_response) const;
	bool _validate_node_type(const String &p_type) const;
	NodeDefinition _parse_node(const Dictionary &p_node_dict) const;
	ScriptDefinition _parse_script(const Dictionary &p_script_dict) const;
	SignalConnection _parse_signal(const Dictionary &p_signal_dict) const;

public:
	ParseResult parse(const String &p_json_string) const;
};

