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

#include "scene/gui/control.h"

class TextEdit;
class Button;
class ProgressBar;
class RichTextLabel;
class Label;
class LLMService;

class NLInputPanel : public Control {
	GDCLASS(NLInputPanel, Control);

	TextEdit *prompt_input = nullptr;
	Button *generate_btn = nullptr;
	Button *clear_btn = nullptr;
	Button *copy_log_btn = nullptr;
	ProgressBar *progress = nullptr;
	RichTextLabel *status_display = nullptr;
	Label *token_label = nullptr;

	Ref<LLMService> llm_service;

	void _setup_ui();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void _on_generate_pressed();
	void _on_clear_pressed();
	void _on_copy_log_pressed();
	void _show_status(const String &p_message, bool p_is_error = false);
};
