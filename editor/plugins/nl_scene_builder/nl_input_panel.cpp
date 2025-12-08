/**************************************************************************/
/*  nl_input_panel.cpp                                                   */
/**************************************************************************/

#include "nl_input_panel.h"

#include "core/input/input_event.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "core/string/string_builder.h"
#include "servers/display/display_server.h"
#include "editor/editor_interface.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"
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
	ClassDB::bind_method(D_METHOD("_on_copy_log_pressed"), &NLInputPanel::_on_copy_log_pressed);
	ClassDB::bind_method(D_METHOD("_on_load_capsule_pressed"), &NLInputPanel::_on_load_capsule_pressed);
	ClassDB::bind_method(D_METHOD("_on_capsule_file_selected", "path"), &NLInputPanel::_on_capsule_file_selected);
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

	HBoxContainer *capsule_row = memnew(HBoxContainer);
	root->add_child(capsule_row);

	load_capsule_btn = memnew(Button);
	load_capsule_btn->set_text("Load Capsule…");
	capsule_row->add_child(load_capsule_btn);

	pass_selector = memnew(OptionButton);
	pass_selector->set_custom_minimum_size(Size2(140, 0));
	pass_selector->add_item("layout");
	pass_selector->add_item("scripts");
	pass_selector->add_item("signals");
	pass_selector->select(0);
	capsule_row->add_child(pass_selector);

	capsule_label = memnew(Label);
	capsule_label->set_text("No capsule loaded");
	capsule_label->set_h_size_flags(SIZE_EXPAND_FILL);
	capsule_row->add_child(capsule_label);

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
	if (load_capsule_btn) {
		load_capsule_btn->connect(SceneStringName(pressed), callable_mp(this, &NLInputPanel::_on_load_capsule_pressed));
	}
	if (pass_selector) {
		pass_selector->connect("item_selected", callable_mp(this, &NLInputPanel::_on_pass_selected));
	}

	capsule_file_dialog = memnew(FileDialog);
	capsule_file_dialog->set_file_mode(FileDialog::FILE_MODE_OPEN_FILE);
	capsule_file_dialog->set_access(FileDialog::ACCESS_FILESYSTEM);
	capsule_file_dialog->clear_filters();
	capsule_file_dialog->add_filter("*.capsule.json ; Scene Prompt Capsules");
	capsule_file_dialog->connect("file_selected", callable_mp(this, &NLInputPanel::_on_capsule_file_selected));
	add_child(capsule_file_dialog);
	_clear_capsule_state();

	// Configure a local LLMService instance. For now we use a best-effort
	// environment lookup and call the OpenAI Chat Completions API when a key
	// is available.
	llm_service.instantiate();
	if (llm_service.is_valid()) {
		String key = _nl_scene_builder_load_api_key();
		if (key.is_empty()) {
			_show_status("No OPENAI_API_KEY found in environment or .env; NL Scene Builder will not call a remote LLM.", true);
		}
		llm_service->configure(key, "gpt-4.1-mini", 16384);
	}

	_show_status("Ready.", false);
}

void NLInputPanel::_clear_capsule_state() {
	capsule_data.clear();
	capsule_path = String();
	if (capsule_label) {
		capsule_label->set_text("No capsule loaded");
	}
	if (pass_selector) {
		pass_selector->clear();
		pass_selector->add_item("layout");
		pass_selector->add_item("scripts");
		pass_selector->add_item("signals");
		pass_selector->select(0);
	}
}

String NLInputPanel::_compose_prompt_from_capsule(const Dictionary &p_capsule, const String &p_active_pass) const {
	auto get_string = [](const Dictionary &dict, const char *key) -> String {
		if (!dict.has(key)) {
			return String();
		}
		Variant value = dict[key];
		return value.get_type() == Variant::STRING ? String(value) : String();
	};

	StringBuilder builder;
	String slug = get_string(p_capsule, "slug");
	String scene_id = get_string(p_capsule, "scene_id");
	String idea = get_string(p_capsule, "idea");
	String scene_goal = get_string(p_capsule, "scene_goal");
	builder.append("SCENE_ID: " + scene_id + "\n");
	builder.append("SLUG: " + slug + "\n");
	builder.append("IDEA: " + idea + "\n");
	if (!p_active_pass.is_empty()) {
		builder.append("ACTIVE_PASS: " + p_active_pass + "\n");
	}
	builder.append("\n");

	Array passes;
	if (p_capsule.has("llm_passes") && Variant(p_capsule["llm_passes"]).get_type() == Variant::ARRAY) {
		passes = p_capsule["llm_passes"];
	}
	if (!passes.is_empty()) {
		builder.append("LLM_PASSES: ");
		for (int i = 0; i < passes.size(); i++) {
			builder.append(String(passes[i]));
			if (i < passes.size() - 1) {
				builder.append(", ");
			}
		}
		builder.append("\n\n");
	}

	if (!scene_goal.is_empty()) {
		builder.append("SCENE_GOAL:\n  " + scene_goal + "\n\n");
	}

	Array required;
	if (p_capsule.has("required_elements") && Variant(p_capsule["required_elements"]).get_type() == Variant::ARRAY) {
		required = p_capsule["required_elements"];
	}
	if (!required.is_empty()) {
		builder.append("REQUIRED_ELEMENTS:\n");
		for (int i = 0; i < required.size(); i++) {
			builder.append("  - " + String(required[i]) + "\n");
		}
		builder.append("\n");
	}

	Array resources;
	if (p_capsule.has("resource_hints") && Variant(p_capsule["resource_hints"]).get_type() == Variant::ARRAY) {
		resources = p_capsule["resource_hints"];
	}
	if (!resources.is_empty()) {
		builder.append("RESOURCE_HINTS:\n");
		for (int i = 0; i < resources.size(); i++) {
			Dictionary res = resources[i];
			String type = get_string(res, "type");
			String path = get_string(res, "path");
			String notes = get_string(res, "notes");
			String line = "  - " + type + ": " + path;
			if (!notes.is_empty()) {
				line += " (" + notes + ")";
			}
			builder.append(line + "\n");
		}
		builder.append("\n");
	}

	Array constraints;
	if (p_capsule.has("constraints") && Variant(p_capsule["constraints"]).get_type() == Variant::ARRAY) {
		constraints = p_capsule["constraints"];
	}
	if (!constraints.is_empty()) {
		builder.append("CONSTRAINTS:\n");
		for (int i = 0; i < constraints.size(); i++) {
			builder.append("  - " + String(constraints[i]) + "\n");
		}
		builder.append("\n");
	}

	if (p_capsule.has("existing_context")) {
		Dictionary ctx = p_capsule["existing_context"];
		Dictionary spec_excerpt;
		if (ctx.has("spec_excerpt") && Variant(ctx["spec_excerpt"]).get_type() == Variant::DICTIONARY) {
			spec_excerpt = ctx["spec_excerpt"];
		}
		if (!spec_excerpt.is_empty()) {
			builder.append("SPEC_EXCERPT:\n");
			Array spec_keys = spec_excerpt.keys();
			for (int i = 0; i < spec_keys.size(); i++) {
				String k = spec_keys[i];
				String v = String(spec_excerpt[spec_keys[i]]);
				builder.append("  - " + k + ": " + v + "\n");
			}
			builder.append("\n");
		}
		Array notes;
		if (ctx.has("notes") && Variant(ctx["notes"]).get_type() == Variant::ARRAY) {
			notes = ctx["notes"];
		}
		if (!notes.is_empty()) {
			builder.append("NOTES:\n");
			for (int i = 0; i < notes.size(); i++) {
				builder.append("  - " + String(notes[i]) + "\n");
			}
			builder.append("\n");
		}
		String summary = get_string(ctx, "scene_summary");
		if (!summary.is_empty()) {
			builder.append("SCENE_SUMMARY:\n  " + summary + "\n\n");
		}
	}

	String prompt_md = get_string(p_capsule, "prompt_md");
	if (!prompt_md.is_empty()) {
		builder.append("PROMPT_MD:\n" + prompt_md + "\n\n");
	}
	String checklist_md = get_string(p_capsule, "checklist_md");
	if (!checklist_md.is_empty()) {
		builder.append("CHECKLIST_MD:\n" + checklist_md + "\n");
	}

	return builder.as_string();
}

void NLInputPanel::_populate_pass_selector(const Array &p_passes) {
	if (!pass_selector) {
		return;
	}
	pass_selector->clear();
	if (p_passes.is_empty()) {
		pass_selector->add_item("layout");
		pass_selector->select(0);
		return;
	}
	for (int i = 0; i < p_passes.size(); i++) {
		String pass_name = String(p_passes[i]);
		if (pass_name.is_empty()) {
			continue;
		}
		pass_selector->add_item(pass_name);
	}
	if (pass_selector->get_item_count() == 0) {
		pass_selector->add_item("layout");
	}
	pass_selector->select(0);
}

void NLInputPanel::_refresh_capsule_prompt() {
	if (!prompt_input || capsule_data.is_empty()) {
		return;
	}
	String active_pass = _get_active_pass();
	prompt_input->set_text(_compose_prompt_from_capsule(capsule_data, active_pass));
}

String NLInputPanel::_get_active_pass() const {
	if (capsule_data.is_empty()) {
		return "full";
	}
	if (pass_selector && pass_selector->get_item_count() > 0) {
		int selected = pass_selector->get_selected();
		if (selected < 0) {
			selected = 0;
		}
		return pass_selector->get_item_text(selected);
	}
	return "layout";
}

void NLInputPanel::_load_capsule_from_path(const String &p_path) {
	if (!FileAccess::exists(p_path)) {
		_show_status("Capsule file not found: " + p_path, true);
		return;
	}

	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	if (file.is_null()) {
		_show_status("Unable to open capsule file: " + p_path, true);
		return;
	}

	String payload = file->get_as_text();
	Ref<JSON> json = memnew(JSON);
	Error err = json->parse(payload);
	if (err != OK) {
		_show_status("Capsule JSON parse error in " + p_path, true);
		return;
	}
	Variant root = json->get_data();
	if (root.get_type() != Variant::DICTIONARY) {
		_show_status("Capsule file must contain a JSON object.", true);
		return;
	}

	capsule_data = root;
	capsule_path = p_path;
	String slug = capsule_data.has("slug") ? String(capsule_data["slug"]) : String();
	String scene_id = capsule_data.has("scene_id") ? String(capsule_data["scene_id"]) : String();
	if (capsule_label) {
		capsule_label->set_text("Capsule: " + slug + "/" + scene_id);
	}
	Array passes;
	if (capsule_data.has("llm_passes") && Variant(capsule_data["llm_passes"]).get_type() == Variant::ARRAY) {
		passes = capsule_data["llm_passes"];
	}
	_populate_pass_selector(passes);
	_refresh_capsule_prompt();
	_show_status("Loaded prompt capsule: " + p_path, false);
}

void NLInputPanel::_on_pass_selected(int p_index) {
	(void)p_index;
	_refresh_capsule_prompt();
}

void NLInputPanel::_on_generate_pressed() {
	if (!prompt_input) {
		return;
	}

	String active_pass = _get_active_pass();
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

	String scene_context;
	if (!capsule_data.is_empty()) {
		Dictionary context = capsule_data.duplicate(true);
		context["requested_pass"] = active_pass;
		scene_context = JSON::stringify(context);
	} else {
		scene_context = "ACTIVE_PASS=" + active_pass;
	}

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
	generator->set_generation_mode(active_pass);
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
	_clear_capsule_state();
	_show_status("Cleared. Ready.", false);
}

void NLInputPanel::_on_load_capsule_pressed() {
	if (!capsule_file_dialog) {
		return;
	}
	if (!capsule_path.is_empty()) {
		capsule_file_dialog->set_current_path(capsule_path);
	} else {
		String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
		String default_dir = (exe_dir.path_join("..").path_join("docs").path_join("artifacts").path_join("scene_prompts")).simplify_path();
		capsule_file_dialog->set_current_dir(default_dir);
	}
	capsule_file_dialog->popup_file_dialog();
}

void NLInputPanel::_on_capsule_file_selected(const String &p_path) {
	_load_capsule_from_path(p_path);
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
