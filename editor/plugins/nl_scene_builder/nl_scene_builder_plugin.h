/* Minimal scaffold for the NL Scene Builder editor plugin.
 *
 * This plugin currently only creates a dock with a label so that
 * we can verify the integration and docking behaviour. Further
 * functionality (input panel, LLM wiring, etc.) will be added
 * in follow-up work.
 */

#pragma once

#include "editor/plugins/editor_plugin.h"

class EditorDock;
class NLInputPanel;

class NLSceneBuilderPlugin : public EditorPlugin {
	GDCLASS(NLSceneBuilderPlugin, EditorPlugin);

	EditorDock *scene_builder_dock = nullptr;
	NLInputPanel *input_panel = nullptr;

public:
	NLSceneBuilderPlugin();
	~NLSceneBuilderPlugin() override;

	String get_plugin_name() const override { return "NL Scene Builder"; }
};
