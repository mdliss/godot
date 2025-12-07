/* LLMService - HTTP client wrapper for NL Scene Builder.
 *
 * This MVP version focuses on configuration and a stubbed request/response
 * lifecycle. The actual HTTP call can be filled in once the target provider
 * and endpoint are finalized.
 */

#pragma once

#include "core/io/http_client_tcp.h"
#include "core/object/ref_counted.h"

#include "nl_types.h"

class LLMService : public RefCounted {
	GDCLASS(LLMService, RefCounted);

	HTTPClientTCP *http_client = nullptr;

	String api_key;
	String model;
	int max_tokens = 4096;
	int timeout_seconds = 90;

	bool is_requesting = false;

	String last_error;
	String response_body;
	HTTPClient::Status last_status = HTTPClient::STATUS_DISCONNECTED;

protected:
	static void _bind_methods();

	String _build_request_body(const String &p_prompt, const String &p_scene_context) const;

public:
	LLMService();
	~LLMService();

	void configure(const String &p_api_key, const String &p_model, int p_max_tokens);

	// Starts a request to the configured LLM provider (currently OpenAI
	// Chat Completions API). This call is synchronous and will block the
	// editor thread until the response is received or a timeout occurs.
	Error send_request(const String &p_prompt, const String &p_scene_context);

	// Polls for a finished response; returns empty string while waiting.
	String poll_response();

	bool is_busy() const { return is_requesting; }

	void cancel();

	String get_last_error() const { return last_error; }
};
