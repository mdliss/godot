/**************************************************************************/
/*  llm_service.cpp                                                      */
/**************************************************************************/

#include "llm_service.h"

#include "core/error/error_list.h"
#include "core/io/json.h"
#include "core/os/os.h"

#include "system_prompt.h"

void LLMService::_bind_methods() {
	ClassDB::bind_method(D_METHOD("configure", "api_key", "model", "max_tokens"), &LLMService::configure);
	ClassDB::bind_method(D_METHOD("send_request", "prompt", "scene_context"), &LLMService::send_request);
	ClassDB::bind_method(D_METHOD("poll_response"), &LLMService::poll_response);
	ClassDB::bind_method(D_METHOD("cancel"), &LLMService::cancel);
	ClassDB::bind_method(D_METHOD("get_last_error"), &LLMService::get_last_error);
}

LLMService::LLMService() {
	http_client = memnew(HTTPClientTCP);
}

LLMService::~LLMService() {
	if (http_client) {
		memdelete(http_client);
		http_client = nullptr;
	}
}

void LLMService::configure(const String &p_api_key, const String &p_model, int p_max_tokens) {
	api_key = p_api_key;
	model = p_model;
	max_tokens = p_max_tokens > 0 ? p_max_tokens : 4096;
}

String LLMService::_build_request_body(const String &p_prompt, const String &p_scene_context) const {
	// Build a JSON payload for OpenAI's chat/completions API.

	const String final_model = model.is_empty() ? String("gpt-4.1-mini") : model;

	Dictionary user_message;
	String content;
	if (!p_scene_context.is_empty()) {
		content += "Current scene context (JSON or description):\n";
		content += p_scene_context;
		content += "\n\n";
	}
	content += "User request:\n";
	content += p_prompt;

	user_message["role"] = "user";
	user_message["content"] = content;

	Dictionary system_message;
	system_message["role"] = "system";
	system_message["content"] = nl_scene_builder_get_system_prompt();

	Array messages;
	messages.push_back(system_message);
	messages.push_back(user_message);

	Dictionary root;
	root["model"] = final_model;
	root["messages"] = messages;
	root["temperature"] = 0.2;
	root["max_tokens"] = max_tokens;

	return JSON::stringify(root);
}

Error LLMService::send_request(const String &p_prompt, const String &p_scene_context) {
	if (api_key.is_empty()) {
		last_error = "LLMService is not configured with an API key.";
		return ERR_UNCONFIGURED;
	}

	if (is_requesting) {
		last_error = "LLMService request already in progress.";
		return ERR_BUSY;
	}

	last_error.clear();
	response_body = String();
	is_requesting = true;

	if (!http_client) {
		http_client = memnew(HTTPClientTCP);
	}

	http_client->close();

	Error err = http_client->connect_to_host("api.openai.com", 443, TLSOptions::client());
	if (err != OK) {
		last_error = "Failed to start connection to OpenAI.";
		is_requesting = false;
		return err;
	}

	const uint64_t timeout_ms = (uint64_t)timeout_seconds * 1000;
	uint64_t start_ms = OS::get_singleton()->get_ticks_msec();

	while (http_client->get_status() == HTTPClient::STATUS_CONNECTING || http_client->get_status() == HTTPClient::STATUS_RESOLVING) {
		http_client->poll();
		OS::get_singleton()->delay_usec(50000);
		if (OS::get_singleton()->get_ticks_msec() - start_ms > timeout_ms) {
			last_error = "Connection to OpenAI timed out.";
			is_requesting = false;
			http_client->close();
			return ERR_TIMEOUT;
		}
	}

	if (http_client->get_status() != HTTPClient::STATUS_CONNECTED) {
		last_error = "Could not connect to OpenAI.";
		is_requesting = false;
		http_client->close();
		return ERR_CANT_CONNECT;
	}

	Vector<String> headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("Authorization: Bearer " + api_key);

	String body = _build_request_body(p_prompt, p_scene_context);
	CharString body_utf8 = body.utf8();
	err = http_client->request(HTTPClient::METHOD_POST, "/v1/chat/completions", headers, (const uint8_t *)body_utf8.get_data(), body_utf8.length());
	if (err != OK) {
		last_error = "Failed to send HTTP request to OpenAI.";
		is_requesting = false;
		http_client->close();
		return err;
	}

	start_ms = OS::get_singleton()->get_ticks_msec();
	while (http_client->get_status() == HTTPClient::STATUS_REQUESTING) {
		http_client->poll();
		OS::get_singleton()->delay_usec(50000);
		if (OS::get_singleton()->get_ticks_msec() - start_ms > timeout_ms) {
			last_error = "OpenAI request timed out.";
			is_requesting = false;
			http_client->close();
			return ERR_TIMEOUT;
		}
	}

	if (!http_client->has_response()) {
		last_error = "No response received from OpenAI.";
		is_requesting = false;
		http_client->close();
		return ERR_CONNECTION_ERROR;
	}

	const int response_code = http_client->get_response_code();
	if (response_code < 200 || response_code >= 300) {
		last_error = "OpenAI returned HTTP error code: " + itos(response_code);
		is_requesting = false;
		http_client->close();
		return ERR_CANT_ACQUIRE_RESOURCE;
	}

	PackedByteArray data;
	while (http_client->get_status() == HTTPClient::STATUS_BODY) {
		http_client->poll();
		PackedByteArray chunk = http_client->read_response_body_chunk();
		if (chunk.is_empty()) {
			OS::get_singleton()->delay_usec(50000);
			continue;
		}
		data.append_array(chunk);
	}

	http_client->close();
	is_requesting = false;

	if (data.is_empty()) {
		last_error = "Empty response body from OpenAI.";
		return ERR_PARSE_ERROR;
	}

	String raw_body = String::utf8((const char *)data.ptr(), data.size());

	Ref<JSON> json = memnew(JSON);
	err = json->parse(raw_body);
	if (err != OK) {
		last_error = "Failed to parse OpenAI response JSON.";
		return ERR_PARSE_ERROR;
	}

	Variant root_var = json->get_data();
	if (root_var.get_type() != Variant::DICTIONARY) {
		last_error = "Unexpected OpenAI response shape.";
		return ERR_PARSE_ERROR;
	}

	Dictionary root = root_var;
	if (!root.has("choices")) {
		last_error = "OpenAI response missing 'choices'.";
		return ERR_PARSE_ERROR;
	}

	Array choices = root["choices"];
	if (choices.is_empty()) {
		last_error = "OpenAI response has empty 'choices'.";
		return ERR_PARSE_ERROR;
	}

	Dictionary first_choice = choices[0];
	if (!first_choice.has("message")) {
		last_error = "OpenAI response choice missing 'message'.";
		return ERR_PARSE_ERROR;
	}

	Dictionary message = first_choice["message"];
	if (!message.has("content")) {
		last_error = "OpenAI response message missing 'content'.";
		return ERR_PARSE_ERROR;
	}

	response_body = (String)message["content"];
	last_status = HTTPClient::STATUS_BODY;
	last_error.clear();

	return OK;
}

String LLMService::poll_response() {
	// In a future implementation, this would advance the HTTPClient state
	// machine. For now, we just return the stubbed response body.
	if (response_body.is_empty()) {
		return String();
	}
	return response_body;
}

void LLMService::cancel() {
	is_requesting = false;
	last_status = HTTPClient::STATUS_DISCONNECTED;
	response_body = String();
}
