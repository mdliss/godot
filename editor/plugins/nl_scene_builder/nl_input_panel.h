/* NLInputPanel - UI dock content for the NL Scene Builder.
 *
 * This is a minimal implementation focused on layout and basic interactions:
 * - Multi-line prompt input.
 * - Generate and Clear buttons.
 * - Status area and progress bar.
 *
 * LLM integration and scene generation are wired later via LLMService and
 * SceneGenerator.
 */

#pragma once

#include "core/variant/dictionary.h"
#include "scene/gui/control.h"

class TextEdit;
class Button;
class ProgressBar;
class RichTextLabel;
class Label;
class FileDialog;
class OptionButton;
class LLMService;

class NLInputPanel : public Control {
	GDCLASS(NLInputPanel, Control);

	TextEdit *prompt_input = nullptr;
	Button *generate_btn = nullptr;
	Button *clear_btn = nullptr;
	Button *copy_log_btn = nullptr;
	Button *load_capsule_btn = nullptr;
	OptionButton *pass_selector = nullptr;
	ProgressBar *progress = nullptr;
	RichTextLabel *status_display = nullptr;
	Label *token_label = nullptr;
	Label *capsule_label = nullptr;
	FileDialog *capsule_file_dialog = nullptr;
	Dictionary capsule_data;
	String capsule_path;

	Ref<LLMService> llm_service;

	void _setup_ui();
	void _clear_capsule_state();
	void _load_capsule_from_path(const String &p_path);
	String _compose_prompt_from_capsule(const Dictionary &p_capsule, const String &p_active_pass) const;
	void _populate_pass_selector(const Array &p_passes);
	void _refresh_capsule_prompt();
	String _get_active_pass() const;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void _on_generate_pressed();
	void _on_clear_pressed();
	void _on_copy_log_pressed();
	void _on_load_capsule_pressed();
	void _on_capsule_file_selected(const String &p_path);
	void _on_pass_selected(int p_index);
	void _show_status(const String &p_message, bool p_is_error = false);
};
