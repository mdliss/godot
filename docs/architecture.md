# Godot NL Scene Builder - System Architecture

## Architecture Overview

This document describes the system architecture for the Godot NL Scene Builder, a C++ editor modification that enables natural language to 2D scene generation. The architecture follows a modular plugin pattern integrated directly into Godot's editor codebase.

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           GODOT EDITOR                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                    NL SCENE BUILDER PLUGIN                            │   │
│  │                    (editor/plugins/nl_scene_builder/)                 │   │
│  ├──────────────────────────────────────────────────────────────────────┤   │
│  │                                                                       │   │
│  │  ┌─────────────────────┐    ┌─────────────────────────────────────┐  │   │
│  │  │   NL Input Panel    │    │        LLM Service                  │  │   │
│  │  │   (UI Components)   │───▶│   (Claude API Integration)          │  │   │
│  │  │                     │◀───│                                     │  │   │
│  │  │  - TextEdit         │    │  - HTTPClient wrapper               │  │   │
│  │  │  - Generate Button  │    │  - Request builder                  │  │   │
│  │  │  - Status Display   │    │  - Response parser                  │  │   │
│  │  │  - Progress Bar     │    │  - Error handling                   │  │   │
│  │  └─────────────────────┘    └─────────────────────────────────────┘  │   │
│  │           │                              │                            │   │
│  │           │                              ▼                            │   │
│  │           │                 ┌─────────────────────────────────────┐  │   │
│  │           │                 │      Response Parser                │  │   │
│  │           │                 │                                     │  │   │
│  │           │                 │  - JSON validation                  │  │   │
│  │           │                 │  - Node definition extraction       │  │   │
│  │           │                 │  - Script extraction                │  │   │
│  │           │                 │  - Signal connection extraction     │  │   │
│  │           │                 └─────────────────────────────────────┘  │   │
│  │           │                              │                            │   │
│  │           ▼                              ▼                            │   │
│  │  ┌───────────────────────────────────────────────────────────────┐   │   │
│  │  │                   Scene Generator                              │   │   │
│  │  │                                                                │   │   │
│  │  │  - Node factory (create nodes by type string)                  │   │   │
│  │  │  - Property applicator (set node properties)                   │   │   │
│  │  │  - Hierarchy builder (parent/child relationships)              │   │   │
│  │  │  - Script attacher (create and attach GDScript)                │   │   │
│  │  │  - Signal connector (connect signals between nodes)            │   │   │
│  │  └───────────────────────────────────────────────────────────────┘   │   │
│  │                              │                                        │   │
│  └──────────────────────────────┼────────────────────────────────────────┘   │
│                                 │                                            │
│                                 ▼                                            │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                     GODOT EDITOR CORE                                 │   │
│  ├──────────────────────────────────────────────────────────────────────┤   │
│  │                                                                       │   │
│  │  ┌─────────────────┐  ┌──────────────────┐  ┌─────────────────────┐  │   │
│  │  │ EditorInterface │  │ EditorUndoRedo   │  │ EditorSettings      │  │   │
│  │  │                 │  │ Manager          │  │                     │  │   │
│  │  │ - get_edited_   │  │                  │  │ - API key storage   │  │   │
│  │  │   scene_root()  │  │ - create_action  │  │ - Model selection   │  │   │
│  │  │ - get_selection │  │ - add_do_method  │  │ - Panel position    │  │   │
│  │  │ - edit_node     │  │ - add_undo_method│  │                     │  │   │
│  │  └─────────────────┘  └──────────────────┘  └─────────────────────┘  │   │
│  │                                                                       │   │
│  │  ┌─────────────────┐  ┌──────────────────┐  ┌─────────────────────┐  │   │
│  │  │ ClassDB         │  │ SceneTree        │  │ ResourceLoader      │  │   │
│  │  │                 │  │                  │  │                     │  │   │
│  │  │ - class_exists  │  │ - Node hierarchy │  │ - Placeholder       │  │   │
│  │  │ - instantiate   │  │ - add_child      │  │   textures          │  │   │
│  │  │ - get_property  │  │ - remove_child   │  │                     │  │   │
│  │  └─────────────────┘  └──────────────────┘  └─────────────────────┘  │   │
│  │                                                                       │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      │ HTTPS
                                      ▼
                    ┌─────────────────────────────────────┐
                    │         CLAUDE API                   │
                    │    (api.anthropic.com/v1/messages)   │
                    │                                      │
                    │  - claude-sonnet-4-20250514 model              │
                    │  - Structured JSON output            │
                    │  - Godot-optimized system prompt     │
                    └─────────────────────────────────────┘
```

---

## System Components

### Plugin Architecture

#### 1. NLSceneBuilderPlugin (EditorPlugin)

**Location:** `editor/plugins/nl_scene_builder/nl_scene_builder_plugin.cpp`

**Purpose:** Main plugin class that registers with Godot's editor

**Responsibilities:**

- Register plugin with editor on load
- Create and manage NL Input Panel dock
- Handle plugin enable/disable
- Clean up resources on unload

**Key Methods:**

```cpp
class NLSceneBuilderPlugin : public EditorPlugin {
    GDCLASS(NLSceneBuilderPlugin, EditorPlugin);

private:
    NLInputPanel *input_panel = nullptr;

public:
    void _enter_tree() override;      // Called when plugin loads
    void _exit_tree() override;       // Called when plugin unloads
    String get_name() const override; // "NL Scene Builder"
};
```

#### 2. NLInputPanel (Control)

**Location:** `editor/plugins/nl_scene_builder/nl_input_panel.cpp`

**Purpose:** User interface for natural language input

**Components:**

- `TextEdit` - Multi-line text input for prompts
- `Button` - "Generate" button to trigger generation
- `Button` - "Clear" button to reset input
- `ProgressBar` - Shows generation progress
- `RichTextLabel` - Displays status messages and errors
- `Label` - Token usage display

**Key Methods:**

```cpp
class NLInputPanel : public Control {
    GDCLASS(NLInputPanel, Control);

private:
    TextEdit *prompt_input = nullptr;
    Button *generate_btn = nullptr;
    Button *clear_btn = nullptr;
    ProgressBar *progress = nullptr;
    RichTextLabel *status_display = nullptr;
    
    LLMService *llm_service = nullptr;
    SceneGenerator *scene_generator = nullptr;

public:
    void _ready() override;
    void _on_generate_pressed();
    void _on_clear_pressed();
    void _on_generation_complete(const NLGenerationResponse &response);
    void _on_generation_error(const String &error);
    void _show_status(const String &message, bool is_error = false);
};
```

#### 3. LLMService

**Location:** `editor/plugins/nl_scene_builder/llm_service.cpp`

**Purpose:** Handle all communication with Claude API

**Responsibilities:**

- Build HTTP requests with proper headers
- Construct system prompt with Godot context
- Send requests via HTTPClient
- Parse JSON responses
- Handle errors (network, API, parsing)

**Key Methods:**

```cpp
class LLMService : public RefCounted {
    GDCLASS(LLMService, RefCounted);

private:
    HTTPClient *http_client = nullptr;
    String api_key;
    String model;
    int max_tokens;

    String _build_system_prompt();
    String _build_scene_context(Node *scene_root);
    Dictionary _parse_response(const String &json_response);

public:
    void configure(const String &api_key, const String &model, int max_tokens);
    Error generate(const String &prompt, Node *scene_root, Callable callback);
    void cancel();
};
```

**System Prompt Structure:**

```
You are a Godot 4 scene generation assistant. You help game developers create 
2D scenes by converting natural language descriptions into structured JSON.

OUTPUT FORMAT:
You must respond with ONLY valid JSON matching this schema:
{
  "nodes": [
    {
      "name": "string",
      "type": "Godot node type (e.g., CharacterBody2D)",
      "parent": "root" or node path,
      "properties": { "property_name": value },
      "children": [ ...nested nodes... ]
    }
  ],
  "scripts": [
    {
      "attach_to": "node path",
      "code": "valid GDScript code"
    }
  ],
  "signals": [
    {
      "source": "node path",
      "signal": "signal name",
      "target": "node path", 
      "method": "method name"
    }
  ]
}

SUPPORTED NODE TYPES:
- CharacterBody2D, RigidBody2D, StaticBody2D, Area2D
- Sprite2D, AnimatedSprite2D
- CollisionShape2D (with RectangleShape2D, CircleShape2D, CapsuleShape2D)
- Camera2D
- TileMapLayer
- Control, Label, Button, ProgressBar, TextureRect, Panel
- Timer, AudioStreamPlayer2D

GDSCRIPT RULES:
- Use Godot 4 syntax (extends, @export, @onready)
- Use tabs for indentation (not spaces)
- Use move_and_slide() without arguments for CharacterBody2D
- Use Input.get_vector() for movement input
- Connect signals in _ready() or via "signals" array

EXAMPLES:
[Include 3-4 complete examples of prompts and expected JSON output]
```

#### 4. ResponseParser

**Location:** `editor/plugins/nl_scene_builder/response_parser.cpp`

**Purpose:** Parse and validate LLM JSON responses

**Responsibilities:**

- Parse JSON string to Dictionary
- Extract node definitions
- Validate node types against ClassDB
- Extract script definitions
- Extract signal connections
- Report parsing errors

**Key Methods:**

```cpp
class ResponseParser : public RefCounted {
    GDCLASS(ResponseParser, RefCounted);

public:
    struct ParseResult {
        bool success;
        String error_message;
        Vector<NodeDefinition> nodes;
        Vector<ScriptDefinition> scripts;
        Vector<SignalConnection> signals;
    };

    ParseResult parse(const String &json_string);
    
private:
    bool _validate_node_type(const String &type);
    NodeDefinition _parse_node(const Dictionary &node_dict);
    ScriptDefinition _parse_script(const Dictionary &script_dict);
    SignalConnection _parse_signal(const Dictionary &signal_dict);
};
```

#### 5. SceneGenerator

**Location:** `editor/plugins/nl_scene_builder/scene_generator.cpp`

**Purpose:** Create Godot nodes from parsed response

**Responsibilities:**

- Create nodes using ClassDB
- Set node properties
- Build node hierarchy
- Generate and attach GDScript files
- Connect signals
- Integrate with undo/redo system
- Handle placeholder resources (textures, shapes)

**Key Methods:**

```cpp
class SceneGenerator : public RefCounted {
    GDCLASS(SceneGenerator, RefCounted);

private:
    EditorInterface *editor_interface = nullptr;
    EditorUndoRedoManager *undo_redo = nullptr;

    Node *_create_node(const NodeDefinition &def, Node *parent);
    void _apply_properties(Node *node, const Dictionary &properties);
    void _attach_script(Node *node, const String &code);
    void _connect_signals(const Vector<SignalConnection> &signals, Node *scene_root);
    Ref<Resource> _create_placeholder_texture(const Color &color, const Vector2 &size);
    Ref<Shape2D> _create_collision_shape(const String &shape_type, const Dictionary &params);

public:
    void set_editor_interface(EditorInterface *p_interface);
    void set_undo_redo(EditorUndoRedoManager *p_undo_redo);
    
    struct GenerationResult {
        bool success;
        int nodes_created;
        Vector<String> warnings;
        String error_message;
    };
    
    GenerationResult generate(const ParseResult &parsed, Node *scene_root);
};
```

---

## Data Flow

### 1. Generation Request Flow

```
User types prompt in TextEdit
         │
         ▼
User clicks "Generate" button
         │
         ▼
NLInputPanel::_on_generate_pressed()
         │
         ├─── Validate input (not empty)
         │
         ├─── Show progress indicator
         │
         ▼
LLMService::generate(prompt, scene_root, callback)
         │
         ├─── Build system prompt
         │
         ├─── Build scene context JSON
         │
         ├─── Construct HTTP request
         │         │
         │         ▼
         │    ┌─────────────────────────────┐
         │    │     Claude API              │
         │    │  api.anthropic.com          │
         │    └─────────────────────────────┘
         │         │
         │         ▼
         ├─── Receive HTTP response
         │
         ├─── Check for HTTP/API errors
         │
         ▼
ResponseParser::parse(json_string)
         │
         ├─── Parse JSON
         │
         ├─── Validate node types
         │
         ├─── Extract nodes, scripts, signals
         │
         ▼
Callback to NLInputPanel with ParseResult
         │
         ▼
SceneGenerator::generate(parsed, scene_root)
         │
         ├─── Begin undo action
         │
         ├─── Create nodes recursively
         │
         ├─── Apply properties
         │
         ├─── Attach scripts
         │
         ├─── Connect signals
         │
         ├─── End undo action
         │
         ▼
NLInputPanel::_on_generation_complete()
         │
         ├─── Hide progress indicator
         │
         ├─── Display success message
         │
         ▼
Scene tree updated with new nodes
```

### 2. Undo/Redo Flow

```
SceneGenerator::generate() starts
         │
         ▼
undo_redo->create_action("NL Scene Builder: Generate")
         │
         ├─── For each node:
         │    undo_redo->add_do_method(parent, "add_child", node)
         │    undo_redo->add_undo_method(parent, "remove_child", node)
         │    undo_redo->add_do_reference(node)
         │
         ├─── For each script:
         │    undo_redo->add_do_method(node, "set_script", script)
         │    undo_redo->add_undo_method(node, "set_script", null)
         │
         ▼
undo_redo->commit_action()
         │
         ▼
User presses Ctrl+Z
         │
         ▼
All nodes removed, scripts detached (single undo step)
```

### 3. Error Handling Flow

```
Error occurs at any stage
         │
         ├─── Network Error (HTTPClient)
         │    └─── "Connection failed" / "Request timed out"
         │
         ├─── API Error (Claude)
         │    ├─── 401: "Invalid API key"
         │    ├─── 429: "Rate limited, please wait"
         │    └─── 500: "API server error"
         │
         ├─── Parse Error (JSON)
         │    └─── "Failed to parse response: [details]"
         │
         ├─── Validation Error (Node types)
         │    └─── "Unknown node type: [type] (skipped)"
         │
         └─── Generation Error (Scene tree)
              └─── "Failed to create node: [details]"
         │
         ▼
Error callback to NLInputPanel
         │
         ▼
_show_status(error_message, is_error=true)
         │
         ▼
RichTextLabel displays error in red
Progress bar hidden, Generate button re-enabled
```

---

## File Structure

```
godot/
├── editor/
│   ├── plugins/
│   │   ├── nl_scene_builder/
│   │   │   ├── nl_scene_builder_plugin.h
│   │   │   ├── nl_scene_builder_plugin.cpp
│   │   │   ├── nl_input_panel.h
│   │   │   ├── nl_input_panel.cpp
│   │   │   ├── llm_service.h
│   │   │   ├── llm_service.cpp
│   │   │   ├── response_parser.h
│   │   │   ├── response_parser.cpp
│   │   │   ├── scene_generator.h
│   │   │   ├── scene_generator.cpp
│   │   │   ├── nl_scene_builder_types.h     # Shared structs
│   │   │   ├── system_prompt.h              # System prompt text
│   │   │   ├── register_types.h
│   │   │   ├── register_types.cpp
│   │   │   └── SCsub                        # SCons build file
│   │   └── SCsub                            # Update to include nl_scene_builder
│   ├── editor_node.cpp                      # May need modification to register plugin
│   └── ...
├── doc/
│   └── classes/
│       ├── NLSceneBuilderPlugin.xml         # Documentation
│       └── ...
└── ...
```

---

## Key Godot APIs Used

### EditorPlugin API

```cpp
// Register dock panel
add_control_to_dock(DOCK_SLOT_RIGHT_UL, input_panel);

// Remove dock panel
remove_control_from_docks(input_panel);

// Get editor interface
EditorInterface *editor = get_editor_interface();
```

### EditorInterface API

```cpp
// Get current scene root
Node *scene_root = editor_interface->get_edited_scene_root();

// Get editor selection
EditorSelection *selection = editor_interface->get_selection();

// Select nodes in editor
selection->clear();
selection->add_node(created_node);

// Edit node in inspector
editor_interface->edit_node(created_node);
```

### EditorUndoRedoManager API

```cpp
// Create undo action
EditorUndoRedoManager *undo_redo = get_undo_redo();
undo_redo->create_action("NL Scene Builder: Generate");

// Add do/undo methods
undo_redo->add_do_method(parent, "add_child", node, true);
undo_redo->add_undo_method(parent, "remove_child", node);
undo_redo->add_do_reference(node);  // Keep node alive for undo

// Commit action
undo_redo->commit_action();
```

### ClassDB API

```cpp
// Check if class exists
bool exists = ClassDB::class_exists(class_name);

// Create instance
Object *obj = ClassDB::instantiate(class_name);
Node *node = Object::cast_to<Node>(obj);

// Check if property exists
bool has_prop = ClassDB::has_property(class_name, property_name);
```

### HTTPClient API

```cpp
HTTPClient *http = memnew(HTTPClient);

// Connect
Error err = http->connect_to_host("api.anthropic.com", 443, TLSOptions::client());

// Wait for connection
while (http->get_status() == HTTPClient::STATUS_CONNECTING) {
    http->poll();
    OS::get_singleton()->delay_usec(100000);
}

// Send request
Vector<String> headers;
headers.push_back("Content-Type: application/json");
headers.push_back("x-api-key: " + api_key);
headers.push_back("anthropic-version: 2023-06-01");

http->request(HTTPClient::METHOD_POST, "/v1/messages", headers, body);

// Read response
while (http->get_status() == HTTPClient::STATUS_BODY) {
    http->poll();
    response_body += http->read_response_body_chunk();
}
```

### JSON API

```cpp
JSON json;
Error err = json.parse(json_string);
if (err != OK) {
    // Handle parse error
    String error_msg = json.get_error_message();
    int error_line = json.get_error_line();
}

Variant data = json.get_data();
Dictionary dict = data;
```

---

## Configuration & Settings

### EditorSettings Integration

```cpp
// Define settings
void NLSceneBuilderPlugin::_define_settings() {
    EDITOR_DEF("nl_scene_builder/api_key", "");
    EditorSettings::get_singleton()->set_initial_value(
        "nl_scene_builder/api_key", "", true);
    EditorSettings::get_singleton()->add_property_hint(PropertyInfo(
        Variant::STRING, "nl_scene_builder/api_key",
        PROPERTY_HINT_PASSWORD));
    
    EDITOR_DEF("nl_scene_builder/model", "claude-sonnet-4-20250514");
    EDITOR_DEF("nl_scene_builder/max_tokens", 4096);
    EDITOR_DEF("nl_scene_builder/request_timeout", 30);
}

// Read settings
String api_key = EDITOR_GET("nl_scene_builder/api_key");
String model = EDITOR_GET("nl_scene_builder/model");
int max_tokens = EDITOR_GET("nl_scene_builder/max_tokens");
```

### Environment Variable Fallback

```cpp
String NLSceneBuilderPlugin::_get_api_key() {
    String key = EDITOR_GET("nl_scene_builder/api_key");
    if (key.is_empty()) {
        key = OS::get_singleton()->get_environment("ANTHROPIC_API_KEY");
    }
    return key;
}
```

---

## Performance Considerations

### HTTP Request Handling

- **Timeout**: 30 second default, configurable
- **Connection reuse**: HTTPClient kept alive for multiple requests
- **Blocking**: Current MVP blocks editor during request (future: async)

### Node Creation

- **Batch operations**: All nodes created before properties set
- **Deferred calls**: Use `call_deferred` for scene tree modifications
- **Memory**: All nodes added to undo_redo reference to prevent premature deletion

### UI Responsiveness

- **Progress indication**: ProgressBar pulses during API call
- **Button state**: Generate button disabled during generation
- **Status updates**: RichTextLabel updated incrementally

---

## Security Considerations

### API Key Storage

- Stored in EditorSettings (user's local config)
- Displayed as password field (hidden input)
- Never logged or displayed in output
- Environment variable fallback for CI/automated builds

### Network Security

- HTTPS only for API communication
- TLS verification enabled
- No sensitive data in error messages shown to user

---

## Testing Strategy

### Manual Testing (MVP)

Since this is a brownfield modification with limited testing infrastructure:

1. **Unit-level testing via print debugging**
   - JSON parsing with various malformed inputs
   - Node type validation
   - Property application

2. **Integration testing**
   - Full generation flow with real API
   - Undo/redo verification
   - Multiple sequential generations

3. **Edge cases**
   - Empty scene
   - Very large prompts
   - Network interruption mid-request
   - Invalid API key

### Test Prompts

```
# Basic player
"Add a player with WASD controls"

# Multiple nodes
"Create a player, 3 coins, and a goal area"

# Complex behavior
"Add an enemy that patrols between two points and chases the player when close"

# UI elements
"Add a health bar in the top left and a score label in the top right"

# Error case - unknown type
"Add a SuperMegaNode3D" (should warn and skip)
```

---

## Future Enhancements (Post-MVP)

### Phase 2: Async & Streaming

- Non-blocking HTTP requests
- Streaming response display
- Cancel button during generation

### Phase 3: Context Awareness

- Read project assets (textures, scenes, scripts)
- Reference existing nodes by name
- Suggest assets from project

### Phase 4: 3D Support

- 3D node types (Node3D, CharacterBody3D, etc.)
- Mesh generation (primitive shapes)
- Material/shader generation

### Phase 5: Advanced Features

- Generation history/undo stack
- Prompt templates/presets
- Fine-tuned model support
- Local LLM option (Ollama)

---

## Build Configuration

### SCsub (SCons build file)

```python
# editor/plugins/nl_scene_builder/SCsub
Import("env")

env_nl_scene_builder = env.Clone()

# Add include paths if needed
env_nl_scene_builder.Prepend(CPPPATH=["#editor/plugins/nl_scene_builder"])

# Compile all source files
env_nl_scene_builder.add_source_files(
    env.editor_sources,
    "*.cpp"
)
```

### Parent SCsub modification

```python
# editor/plugins/SCsub
# Add this line:
SConscript("nl_scene_builder/SCsub")
```

### Build command

```bash
scons platform=macos arch=arm64 -j10
```

---

## References

- Godot Engine Source: https://github.com/godotengine/godot
- Godot EditorPlugin docs: https://docs.godotengine.org/en/stable/classes/class_editorplugin.html
- Claude API docs: https://docs.anthropic.com/en/api/messages
- Godot C++ contribution guidelines: https://docs.godotengine.org/en/stable/contributing/development/code_style_guidelines.html