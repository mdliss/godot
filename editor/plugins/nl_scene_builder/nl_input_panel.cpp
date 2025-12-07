/**************************************************************************/
/*  nl_input_panel.cpp                                                   */
/**************************************************************************/

#include "nl_input_panel.h"

#include "core/input/input_event.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "servers/display/display_server.h"
#include "editor/editor_interface.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/progress_bar.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/separator.h"
#include "scene/gui/text_edit.h"

#include "llm_service.h"
#include "response_parser.h"
#include "scene_generator.h"

static String _nl_scene_builder_load_api_key() {
	// 1) Environment variables.
	String key = OS::get_singleton()->get_environment("OPENAI_API_KEY");
	if (key.is_empty()) {
		key = OS::get_singleton()->get_environment("ANTHROPIC_API_KEY");
	}
	if (!key.is_empty()) {
		return key.strip_edges();
	}

	// 2) .env next to the engine source root (one directory above the
	//    executable's directory), matching this repo layout.
	String exe_path = OS::get_singleton()->get_executable_path();
	String exe_dir = exe_path.get_base_dir();
	String env_path = (exe_dir.path_join("..").path_join(".env")).simplify_path();

	print_line("NL Scene Builder: Looking for .env at: " + env_path);

	if (!FileAccess::exists(env_path)) {
		print_line("NL Scene Builder: .env not found at that path");
		return String();
	}

	Ref<FileAccess> f = FileAccess::open(env_path, FileAccess::READ);
	if (f.is_null()) {
		return String();
	}

	while (!f->eof_reached()) {
		String line = f->get_line().strip_edges();
		if (line.is_empty() || line.begins_with("#")) {
			continue;
		}
		if (line.begins_with("OPENAI_API_KEY=")) {
			String value = line.substr(String("OPENAI_API_KEY=").length()).strip_edges();
			print_line("NL Scene Builder: Found API key, length: " + itos(value.length()));
			if (!value.is_empty()) {
				return value;
			}
		}
	}

	print_line("NL Scene Builder: No OPENAI_API_KEY= line found in .env");
	return String();
}

void NLInputPanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_generate_pressed"), &NLInputPanel::_on_generate_pressed);
	ClassDB::bind_method(D_METHOD("_on_clear_pressed"), &NLInputPanel::_on_clear_pressed);
	ClassDB::bind_method(D_METHOD("_show_status", "message", "is_error"), &NLInputPanel::_show_status, DEFVAL(false));
}

void NLInputPanel::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_setup_ui();
		} break;
	}
}

void NLInputPanel::_setup_ui() {
	set_anchors_and_offsets_preset(PRESET_FULL_RECT);

	VBoxContainer *root = memnew(VBoxContainer);
	root->set_h_size_flags(SIZE_EXPAND_FILL);
	root->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(root);

	Label *title = memnew(Label);
	title->set_text("NL Scene Builder");
	root->add_child(title);

	prompt_input = memnew(TextEdit);
	prompt_input->set_h_size_flags(SIZE_EXPAND_FILL);
	prompt_input->set_v_size_flags(SIZE_EXPAND_FILL);
	prompt_input->set_custom_minimum_size(Size2(400, 300));
	prompt_input->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	prompt_input->set_placeholder("Describe what you want to generate in this scene...");
	root->add_child(prompt_input);

	HBoxContainer *buttons = memnew(HBoxContainer);
	root->add_child(buttons);

	generate_btn = memnew(Button);
	generate_btn->set_text("Generate");
	buttons->add_child(generate_btn);

	clear_btn = memnew(Button);
	clear_btn->set_text("Clear");
	buttons->add_child(clear_btn);

	HSeparator *sep = memnew(HSeparator);
	root->add_child(sep);

	HBoxContainer *log_header = memnew(HBoxContainer);
	root->add_child(log_header);

	Label *log_label = memnew(Label);
	log_label->set_text("Debug Log");
	log_label->set_h_size_flags(SIZE_EXPAND_FILL);
	log_header->add_child(log_label);

	copy_log_btn = memnew(Button);
	copy_log_btn->set_text("Copy Log");
	log_header->add_child(copy_log_btn);

	status_display = memnew(RichTextLabel);
	status_display->set_v_size_flags(SIZE_EXPAND_FILL);
	status_display->set_custom_minimum_size(Size2(0, 200));
	status_display->set_scroll_active(true);
	status_display->set_selection_enabled(true);
	root->add_child(status_display);

	progress = memnew(ProgressBar);
	progress->set_max(1.0);
	progress->set_step(0.0);
	progress->set_visible(false);
	root->add_child(progress);

	token_label = memnew(Label);
	token_label->set_text("Tokens: 0/0");
	root->add_child(token_label);

	if (generate_btn) {
		generate_btn->connect(SceneStringName(pressed), callable_mp(this, &NLInputPanel::_on_generate_pressed));
	}
	if (clear_btn) {
		clear_btn->connect(SceneStringName(pressed), callable_mp(this, &NLInputPanel::_on_clear_pressed));
	}
	if (copy_log_btn) {
		copy_log_btn->connect(SceneStringName(pressed), callable_mp(this, &NLInputPanel::_on_copy_log_pressed));
	}

	// Configure a local LLMService instance. For now we use a best-effort
	// environment lookup and call the OpenAI Chat Completions API when a key
	// is available.
	llm_service.instantiate();
	if (llm_service.is_valid()) {
		String key = _nl_scene_builder_load_api_key();
		if (key.is_empty()) {
			_show_status("No OPENAI_API_KEY found in environment or .env; NL Scene Builder will not call a remote LLM.", true);
		}
		llm_service->configure(key, "gpt-4.1-mini", 8192);
	}

	_show_status("Ready.", false);
}

void NLInputPanel::_on_generate_pressed() {
	if (!prompt_input) {
		return;
	}

	const String prompt = prompt_input->get_text().strip_edges();
	if (prompt.is_empty()) {
		_show_status("Please enter a prompt before generating.", true);
		return;
	}

	if (progress) {
		progress->set_visible(true);
	}

	if (!llm_service.is_valid()) {
		_show_status("LLMService is not available; cannot send request.", true);
		if (progress) {
			progress->set_visible(false);
		}
		return;
	}

	const String scene_context; // To be filled with real scene info later.
	Error err = llm_service->send_request(prompt, scene_context);
	if (err != OK) {
		_show_status("LLM request error: " + llm_service->get_last_error(), true);
		if (progress) {
			progress->set_visible(false);
		}
		return;
	}

	// In the current stub implementation, a response is available immediately.
	String response = llm_service->poll_response();
	if (response.is_empty()) {
		_show_status("LLM request sent (stub); no response body available yet.", false);
		if (progress) {
			progress->set_visible(false);
		}
		return;
	}

	// Parse the stub response into a ParseResult.
	Ref<ResponseParser> parser;
	parser.instantiate();
	ParseResult parsed = parser->parse(response);
	if (!parsed.success) {
		_show_status("Failed to parse LLM response: " + parsed.error_message, true);
		if (progress) {
			progress->set_visible(false);
		}
		return;
	}

	// Find current scene root.
	EditorInterface *editor_interface = EditorInterface::get_singleton();
	if (!editor_interface) {
		_show_status("No EditorInterface singleton available; cannot generate scene content.", true);
		if (progress) {
			progress->set_visible(false);
		}
		return;
	}

	Node *scene_root = editor_interface->get_edited_scene_root();
	if (!scene_root) {
		_show_status("No edited scene open. Please open or create a scene before generating.", true);
		if (progress) {
			progress->set_visible(false);
		}
		return;
	}

	Ref<SceneGenerator> generator;
	generator.instantiate();
	generator->set_editor_interface(editor_interface);
	if (editor_interface) {
		generator->set_undo_redo(editor_interface->get_editor_undo_redo());
	}
	SceneGenerator::GenerationResult gen_result = generator->generate(parsed, scene_root);

	if (!gen_result.success) {
		_show_status("Scene generation failed: " + gen_result.error_message, true);
	} else {
		String msg = "Stub generation created " + itos(gen_result.nodes_created) + " node(s).";
		_show_status(msg, false);
		for (const String &w : gen_result.warnings) {
			_show_status("Warning: " + w, true);
		}
	}

	if (progress) {
		progress->set_visible(false);
	}
}

void NLInputPanel::_on_clear_pressed() {
	if (prompt_input) {
		prompt_input->clear();
	}
	if (status_display) {
		status_display->clear();
	}
	if (progress) {
		progress->set_visible(false);
	}
	_show_status("Cleared. Ready.", false);
}

void NLInputPanel::_on_copy_log_pressed() {
	if (!status_display) {
		return;
	}
	String text = status_display->get_text();
	DisplayServer::get_singleton()->clipboard_set(text);
	_show_status("Log copied to clipboard.", false);
}

void NLInputPanel::_show_status(const String &p_message, bool p_is_error) {
	if (!status_display) {
		return;
	}

	if (p_is_error) {
		status_display->push_color(Color(1.0, 0.4, 0.4));
	} else {
		status_display->push_color(Color(1.0, 1.0, 1.0));
	}

	status_display->add_text(p_message + "\n");
	status_display->pop();
	status_display->scroll_to_line(status_display->get_line_count() - 1);
}
