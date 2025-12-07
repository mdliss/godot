/**************************************************************************/
/*  nl_scene_builder_plugin.cpp                                          */
/**************************************************************************/

#include "nl_scene_builder_plugin.h"

#include "core/input/shortcut.h"
#include "editor/docks/editor_dock.h"
#include "scene/gui/control.h"

#include "nl_input_panel.h"

NLSceneBuilderPlugin::NLSceneBuilderPlugin() {
	scene_builder_dock = memnew(EditorDock);
	scene_builder_dock->set_visible(false);
	scene_builder_dock->set_name("NL Scene Builder");
	scene_builder_dock->set_title("NL Scene Builder");
	scene_builder_dock->set_layout_key("nl_scene_builder");
	scene_builder_dock->set_default_slot(DockConstants::DOCK_SLOT_RIGHT_UL);

	input_panel = memnew(NLInputPanel);
	input_panel->set_name("NL Scene Builder Panel");
	input_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	input_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	scene_builder_dock->add_child(input_panel);

	add_dock(scene_builder_dock);
}

NLSceneBuilderPlugin::~NLSceneBuilderPlugin() {
	if (scene_builder_dock) {
		remove_dock(scene_builder_dock);
		memdelete(scene_builder_dock);
		scene_builder_dock = nullptr;
		input_panel = nullptr;
	}
}
