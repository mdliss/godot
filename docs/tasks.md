# Godot NL Scene Builder - Development Task List

## Project File Structure

```
godot/                                    # Forked Godot repository
├── editor/
│   ├── plugins/
│   │   ├── nl_scene_builder/            # NEW: Our plugin directory
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
│   │   │   ├── nl_types.h               # Shared type definitions
│   │   │   ├── system_prompt.h          # LLM system prompt
│   │   │   ├── register_types.h
│   │   │   ├── register_types.cpp
│   │   │   └── SCsub                    # Build configuration
│   │   └── SCsub                        # MODIFY: Add nl_scene_builder
│   ├── editor_node.h                    # REFERENCE: Plugin registration
│   └── editor_node.cpp
├── core/
│   └── io/
│       ├── http_client.h                # REFERENCE: HTTP implementation
│       └── http_client.cpp
├── doc/
│   └── classes/                         # Future: XML documentation
├── bin/
│   └── godot.macos.editor.arm64         # Built editor binary
├── PRD.md                               # Project requirements
├── ARCHITECTURE.md                      # System architecture
├── TASKS.md                             # This file
└── BRAINLIFT.md                         # Daily development log
```

---

## PR #1: Build Verification & Codebase Exploration

**Branch:** `setup/build-verification`  
**Goal:** Verify build works, understand editor plugin architecture

### Tasks:

- [ ] **1.1: Fork and Clone Repository**
  - Fork https://github.com/godotengine/godot
  - Clone to local machine
  - Verify on correct branch (master for Godot 4.x)

- [ ] **1.2: Initial Build**
  - Install dependencies: `brew install scons`
  - Verify Xcode CLI tools: `xcode-select --install`
  - Build: `scons platform=macos arch=arm64 -j10`
  - Expected time: 15-30 minutes

- [ ] **1.3: Verify Editor Runs**
  - Run: `./bin/godot.macos.editor.arm64`
  - Create new project
  - Verify 2D scene creation works
  - Test undo/redo functionality

- [ ] **1.4: Explore Editor Plugin Architecture**
  - Study `editor/plugins/` directory structure
  - Read existing plugin (e.g., `animation_player_editor_plugin.cpp`)
  - Identify key patterns:
    - EditorPlugin inheritance
    - Dock registration
    - EditorInterface usage
  - Document findings in BRAINLIFT.md

- [ ] **1.5: Explore HTTPClient**
  - Study `core/io/http_client.h` and `.cpp`
  - Understand connection flow
  - Identify TLS usage
  - Document API patterns

- [ ] **1.6: Create Documentation Files**
  - Create PRD.md (from this document)
  - Create ARCHITECTURE.md
  - Create TASKS.md (this file)
  - Create BRAINLIFT.md template

**PR Checklist:**

- [ ] Godot builds successfully
- [ ] Editor launches without errors
- [ ] Understand plugin registration pattern
- [ ] Understand HTTPClient API
- [ ] Documentation files created

---

## PR #2: Plugin Skeleton & Dock Panel

**Branch:** `feature/plugin-skeleton`  
**Goal:** Create empty plugin that registers a dock panel in editor

### Tasks:

- [ ] **2.1: Create Plugin Directory Structure**
  - Create `editor/plugins/nl_scene_builder/`
  - Create all header files (empty)
  - Create all cpp files (empty/minimal)
  - Create SCsub build file

- [ ] **2.2: Implement register_types**
  - Files: `register_types.h`, `register_types.cpp`
  - Register NLSceneBuilderPlugin class
  - Follow pattern from other plugins

```cpp
// register_types.h
void register_nl_scene_builder_types();
void unregister_nl_scene_builder_types();

// register_types.cpp
#include "register_types.h"
#include "nl_scene_builder_plugin.h"
#include "core/object/class_db.h"

void register_nl_scene_builder_types() {
    GDREGISTER_CLASS(NLSceneBuilderPlugin);
}

void unregister_nl_scene_builder_types() {
}
```

- [ ] **2.3: Implement NLSceneBuilderPlugin Base**
  - Files: `nl_scene_builder_plugin.h`, `nl_scene_builder_plugin.cpp`
  - Inherit from EditorPlugin
  - Implement `_enter_tree()` and `_exit_tree()`
  - Register empty dock panel

```cpp
// nl_scene_builder_plugin.h
#ifndef NL_SCENE_BUILDER_PLUGIN_H
#define NL_SCENE_BUILDER_PLUGIN_H

#include "editor/editor_plugin.h"

class NLInputPanel;

class NLSceneBuilderPlugin : public EditorPlugin {
    GDCLASS(NLSceneBuilderPlugin, EditorPlugin);

private:
    NLInputPanel *input_panel = nullptr;

public:
    void _enter_tree() override;
    void _exit_tree() override;
    String get_name() const override { return "NL Scene Builder"; }
};

#endif
```

- [ ] **2.4: Create Basic NLInputPanel**
  - Files: `nl_input_panel.h`, `nl_input_panel.cpp`
  - Inherit from Control
  - Add Label saying "NL Scene Builder"
  - Basic layout setup

- [ ] **2.5: Configure SCsub**
  - Create `editor/plugins/nl_scene_builder/SCsub`:

```python
Import("env")

env_nl = env.Clone()
env_nl.add_source_files(env.editor_sources, "*.cpp")
```

- [ ] **2.6: Update Parent SCsub**
  - Modify `editor/plugins/SCsub`
  - Add: `SConscript("nl_scene_builder/SCsub")`

- [ ] **2.7: Rebuild and Test**
  - Rebuild: `scons platform=macos arch=arm64 -j10`
  - Launch editor
  - Verify dock panel appears (Project > Tools > NL Scene Builder or similar)

**PR Checklist:**

- [ ] Plugin compiles without errors
- [ ] Plugin appears in editor plugin list
- [ ] Dock panel visible in editor
- [ ] No crashes on editor startup/shutdown
- [ ] Clean commit history

---

## PR #3: Input Panel UI

**Branch:** `feature/input-panel-ui`  
**Goal:** Complete UI for natural language input panel

### Tasks:

- [ ] **3.1: Design Panel Layout**
  - Sketch UI layout:
    ```
    ┌─────────────────────────┐
    │ NL Scene Builder        │
    ├─────────────────────────┤
    │ ┌─────────────────────┐ │
    │ │                     │ │
    │ │   TextEdit          │ │
    │ │   (prompt input)    │ │
    │ │                     │ │
    │ └─────────────────────┘ │
    │ [Generate]    [Clear]   │
    │ ─────────────────────── │
    │ Status: Ready           │
    │ ░░░░░░░░░░ (progress)   │
    │                         │
    │ Tokens: 0/4096          │
    └─────────────────────────┘
    ```

- [ ] **3.2: Create UI Components**
  - Files: Update `nl_input_panel.cpp`
  - Add VBoxContainer for layout
  - Add TextEdit for prompt input
  - Add HBoxContainer for buttons
  - Add Generate Button
  - Add Clear Button
  - Add HSeparator
  - Add RichTextLabel for status
  - Add ProgressBar
  - Add Label for token count

- [ ] **3.3: Implement Button Signals**
  - Connect Generate button to `_on_generate_pressed()`
  - Connect Clear button to `_on_clear_pressed()`
  - Implement `_on_clear_pressed()` (clear TextEdit)

- [ ] **3.4: Implement Status Display**
  - Method: `_show_status(String message, bool is_error)`
  - Normal messages in white/default
  - Error messages in red
  - Auto-scroll to bottom

- [ ] **3.5: Implement Progress Indication**
  - Show ProgressBar during generation
  - Hide when idle
  - Indeterminate mode (pulse)

- [ ] **3.6: Style Adjustments**
  - Set minimum sizes
  - Add margins/padding
  - Ensure proper resizing in dock

- [ ] **3.7: Keyboard Shortcuts**
  - Ctrl+Enter to generate (while in TextEdit)
  - Escape to clear

**PR Checklist:**

- [ ] All UI components visible
- [ ] Clear button resets TextEdit
- [ ] Status display works (test with placeholder)
- [ ] Progress bar shows/hides correctly
- [ ] Panel resizes properly in dock
- [ ] Keyboard shortcuts work

---

## PR #4: LLM Service - HTTP Communication

**Branch:** `feature/llm-service`  
**Goal:** Implement HTTP communication with Claude API

### Tasks:

- [ ] **4.1: Create nl_types.h**
  - Shared type definitions for the plugin:

```cpp
#ifndef NL_TYPES_H
#define NL_TYPES_H

#include "core/variant/dictionary.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

struct NodeDefinition {
    String name;
    String type;
    String parent;
    Dictionary properties;
    Vector<NodeDefinition> children;
};

struct ScriptDefinition {
    String attach_to;
    String code;
};

struct SignalConnection {
    String source_node;
    String signal_name;
    String target_node;
    String method_name;
};

struct ParseResult {
    bool success = false;
    String error_message;
    Vector<NodeDefinition> nodes;
    Vector<ScriptDefinition> scripts;
    Vector<SignalConnection> signals;
};

#endif
```

- [ ] **4.2: Implement LLMService Header**
  - Files: `llm_service.h`

```cpp
#ifndef LLM_SERVICE_H
#define LLM_SERVICE_H

#include "core/object/ref_counted.h"
#include "core/io/http_client.h"
#include "nl_types.h"

class LLMService : public RefCounted {
    GDCLASS(LLMService, RefCounted);

private:
    HTTPClient *http_client = nullptr;
    String api_key;
    String model = "claude-sonnet-4-20250514";
    int max_tokens = 4096;
    int timeout_seconds = 30;
    
    bool is_requesting = false;

    String _build_request_body(const String &prompt, const String &scene_context);
    String _get_system_prompt();

public:
    LLMService();
    ~LLMService();

    void configure(const String &p_api_key, const String &p_model, int p_max_tokens);
    Error send_request(const String &prompt, const String &scene_context);
    String poll_response(); // Returns empty string while waiting, response when complete
    bool is_busy() const { return is_requesting; }
    void cancel();
    String get_last_error() const;
    
private:
    String last_error;
    String response_body;
    HTTPClient::Status last_status;
};

#endif
```

- [ ] **4.3: Implement LLMService Core**
  - Files: `llm_service.cpp`
  - Implement constructor/destructor
  - Implement `configure()`
  - Implement `_get_system_prompt()` (basic version)

- [ ] **4.4: Implement HTTP Request**
  - Implement `send_request()`
  - Build proper headers:
    - `Content-Type: application/json`
    - `x-api-key: [key]`
    - `anthropic-version: 2023-06-01`
  - Build request body with system prompt and user message

- [ ] **4.5: Implement Response Polling**
  - Implement `poll_response()`
  - Handle HTTPClient status states
  - Accumulate response body
  - Detect completion

- [ ] **4.6: Implement Error Handling**
  - Handle connection errors
  - Handle HTTP error codes
  - Handle timeout
  - Store errors in `last_error`

- [ ] **4.7: Create system_prompt.h**
  - Store system prompt as const String
  - Include JSON schema
  - Include node type list
  - Include GDScript examples

- [ ] **4.8: Test HTTP Communication**
  - Add test button to panel (temporary)
  - Send simple request
  - Print response to console
  - Verify API communication works

**PR Checklist:**

- [ ] HTTPClient connects to api.anthropic.com
- [ ] Request sends with proper headers
- [ ] Response received and parsed
- [ ] Errors handled gracefully
- [ ] API key configurable
- [ ] Test request/response works

---

## PR #5: Response Parser

**Branch:** `feature/response-parser`  
**Goal:** Parse LLM JSON responses into structured data

### Tasks:

- [ ] **5.1: Implement ResponseParser Header**
  - Files: `response_parser.h`

```cpp
#ifndef RESPONSE_PARSER_H
#define RESPONSE_PARSER_H

#include "core/object/ref_counted.h"
#include "nl_types.h"

class ResponseParser : public RefCounted {
    GDCLASS(ResponseParser, RefCounted);

public:
    ParseResult parse(const String &json_string);

private:
    NodeDefinition _parse_node(const Dictionary &node_dict);
    ScriptDefinition _parse_script(const Dictionary &script_dict);
    SignalConnection _parse_signal(const Dictionary &signal_dict);
    bool _validate_node_type(const String &type);
    Vector<NodeDefinition> _parse_children(const Array &children_array);
};

#endif
```

- [ ] **5.2: Implement JSON Parsing**
  - Files: `response_parser.cpp`
  - Use Godot's JSON class
  - Parse top-level structure
  - Extract "nodes", "scripts", "signals" arrays

- [ ] **5.3: Implement Node Definition Parsing**
  - Parse name, type, parent
  - Parse properties Dictionary
  - Recursively parse children
  - Handle missing optional fields

- [ ] **5.4: Implement Node Type Validation**
  - Use ClassDB::class_exists()
  - Maintain list of supported 2D types
  - Log warnings for unsupported types
  - Continue parsing (skip invalid nodes)

- [ ] **5.5: Implement Script Parsing**
  - Parse attach_to path
  - Parse code string
  - Basic validation (non-empty)

- [ ] **5.6: Implement Signal Parsing**
  - Parse source, signal, target, method
  - Validate required fields

- [ ] **5.7: Error Handling**
  - Invalid JSON format
  - Missing required fields
  - Type mismatches
  - Return detailed error messages

- [ ] **5.8: Test Parser**
  - Test with valid JSON samples
  - Test with malformed JSON
  - Test with missing fields
  - Test with unknown node types

**PR Checklist:**

- [ ] Valid JSON parses correctly
- [ ] Nodes extracted with all properties
- [ ] Scripts extracted
- [ ] Signals extracted
- [ ] Unknown node types logged and skipped
- [ ] Malformed JSON returns error
- [ ] Recursive children parsing works

---

## PR #6: Scene Generator

**Branch:** `feature/scene-generator`  
**Goal:** Create Godot nodes from parsed response

### Tasks:

- [ ] **6.1: Implement SceneGenerator Header**
  - Files: `scene_generator.h`

```cpp
#ifndef SCENE_GENERATOR_H
#define SCENE_GENERATOR_H

#include "core/object/ref_counted.h"
#include "editor/editor_interface.h"
#include "editor/editor_undo_redo_manager.h"
#include "nl_types.h"

class SceneGenerator : public RefCounted {
    GDCLASS(SceneGenerator, RefCounted);

private:
    EditorInterface *editor_interface = nullptr;
    EditorUndoRedoManager *undo_redo = nullptr;
    
    Vector<String> warnings;

    Node *_create_node(const NodeDefinition &def);
    void _apply_properties(Node *node, const Dictionary &properties);
    void _attach_script(Node *node, const String &code);
    Ref<Resource> _create_placeholder_texture(const Color &color, const Vector2i &size);
    Ref<Shape2D> _create_collision_shape(const String &type, const Dictionary &params);

public:
    struct GenerationResult {
        bool success;
        int nodes_created;
        Vector<String> warnings;
        String error_message;
    };

    void set_editor_interface(EditorInterface *p_interface);
    void set_undo_redo(EditorUndoRedoManager *p_undo_redo);
    
    GenerationResult generate(const ParseResult &parsed, Node *scene_root);
};

#endif
```

- [ ] **6.2: Implement Node Factory**
  - Files: `scene_generator.cpp`
  - Use ClassDB::instantiate()
  - Set node name
  - Handle creation errors

- [ ] **6.3: Implement Hierarchy Builder**
  - Find parent node by path
  - Add child nodes
  - Process children recursively
  - Handle "root" as scene root

- [ ] **6.4: Implement Property Applicator**
  - Handle common property types:
    - Vector2 (position, scale)
    - float (rotation)
    - Color (modulate)
    - String (text for Label)
    - int/float (numeric properties)
  - Use node->set() method
  - Skip unknown properties with warning

- [ ] **6.5: Implement Placeholder Resources**
  - Create colored rectangle texture for Sprite2D
  - Use ImageTexture with generated Image
  - Create collision shapes:
    - RectangleShape2D
    - CircleShape2D
    - CapsuleShape2D

- [ ] **6.6: Implement Script Attachment**
  - Create GDScript resource
  - Set source code
  - Attach to node
  - Handle syntax errors

- [ ] **6.7: Implement Signal Connections**
  - Find source and target nodes
  - Connect signal to method
  - Handle missing nodes/signals

- [ ] **6.8: Implement Undo/Redo Integration**
  - Create single undo action for entire generation
  - Add do/undo methods for each node
  - Add references to prevent deletion
  - Commit action after all nodes created

- [ ] **6.9: Test Generation**
  - Generate single node
  - Generate hierarchy
  - Generate with properties
  - Generate with scripts
  - Test undo/redo

**PR Checklist:**

- [ ] Nodes create in scene tree
- [ ] Properties apply correctly
- [ ] Hierarchy builds correctly
- [ ] Scripts attach and parse
- [ ] Placeholder textures appear
- [ ] Collision shapes create
- [ ] Undo reverts entire generation
- [ ] Redo restores generation

---

## PR #7: Integration & Generation Flow

**Branch:** `feature/integration`  
**Goal:** Connect all components into complete generation flow

### Tasks:

- [ ] **7.1: Connect Components in NLInputPanel**
  - Instantiate LLMService
  - Instantiate ResponseParser
  - Instantiate SceneGenerator
  - Wire up references

- [ ] **7.2: Implement Generation Flow**
  - `_on_generate_pressed()`:
    1. Validate input not empty
    2. Get API key from settings
    3. Show progress
    4. Build scene context
    5. Call LLMService
    6. Handle response

- [ ] **7.3: Implement Scene Context Builder**
  - Get current scene root
  - Build JSON of existing nodes
  - Include node names, types, positions
  - Limit depth for token efficiency

- [ ] **7.4: Implement Response Handling**
  - Parse response with ResponseParser
  - Generate scene with SceneGenerator
  - Display success/error status
  - Update token count display

- [ ] **7.5: Implement Poll Loop**
  - Use `_process()` to poll LLMService
  - Update progress bar
  - Handle completion
  - Handle timeout

- [ ] **7.6: Implement Cancel Functionality**
  - Add Cancel button (visible during generation)
  - Call LLMService::cancel()
  - Reset UI state

- [ ] **7.7: Add Editor Settings**
  - API key (password field)
  - Model selection
  - Max tokens
  - Timeout

- [ ] **7.8: End-to-End Testing**
  - Test: "Add a blue rectangle sprite"
  - Test: "Add a player with WASD controls"
  - Test: "Create an enemy that patrols"
  - Test: "Add a coin collectible"
  - Test with existing scene content

**PR Checklist:**

- [ ] Full generation flow works
- [ ] Progress shows during generation
- [ ] Success message displays node count
- [ ] Errors display clearly
- [ ] Cancel stops generation
- [ ] Settings work correctly
- [ ] Works with existing scene content

---

## PR #8: GDScript Templates & System Prompt Refinement

**Branch:** `feature/script-templates`  
**Goal:** Improve system prompt and script generation quality

### Tasks:

- [ ] **8.1: Refine System Prompt**
  - Add comprehensive JSON schema
  - Add more node type examples
  - Add GDScript patterns
  - Include Godot 4 specific syntax

- [ ] **8.2: Add Script Templates to Prompt**
  - Player movement (CharacterBody2D)
  - Patrol AI
  - Collectible pickup
  - Health bar update
  - Score tracking
  - Camera follow

- [ ] **8.3: Test and Iterate Prompts**
  - Test various input prompts
  - Analyze failures
  - Refine system prompt
  - Document effective patterns

- [ ] **8.4: Add Input Validation**
  - Validate prompt length
  - Warn about very short prompts
  - Suggest more detail if needed

- [ ] **8.5: Add Generation History** (Optional)
  - Store last 5 generations
  - Display in panel (collapsible)
  - Allow re-generation

**PR Checklist:**

- [ ] Common game patterns generate correctly
- [ ] Scripts run without errors
- [ ] Movement works with default inputs
- [ ] Area2D signals connect properly
- [ ] UI elements update correctly

---

## PR #9: Polish, Error Handling & Edge Cases

**Branch:** `fix/polish`  
**Goal:** Handle edge cases, improve error messages, polish UX

### Tasks:

- [ ] **9.1: Improve Error Messages**
  - Network errors
  - API errors (rate limit, auth)
  - Parse errors
  - Generation errors
  - Make all messages actionable

- [ ] **9.2: Handle Edge Cases**
  - No scene open
  - Empty scene
  - Very large existing scene
  - Rapid repeated generations
  - Generation while generation in progress

- [ ] **9.3: UI Polish**
  - Loading states
  - Button disabled states
  - Status message formatting
  - Panel minimum size
  - Scroll behavior

- [ ] **9.4: Performance Testing**
  - Test with large scenes
  - Monitor memory usage
  - Check for leaks
  - Optimize if needed

- [ ] **9.5: Keyboard Navigation**
  - Tab between controls
  - Focus management
  - Accessibility

- [ ] **9.6: Documentation**
  - Add tooltips to UI elements
  - Add placeholder text in TextEdit
  - Help text for first-time users

**PR Checklist:**

- [ ] All error cases have clear messages
- [ ] No crashes on edge cases
- [ ] UI feels responsive
- [ ] Memory stable after many generations
- [ ] Keyboard navigation works

---

## PR #10: Documentation & Demo

**Branch:** `docs/final`  
**Goal:** Complete documentation and prepare demo

### Tasks:

- [ ] **10.1: Update README**
  - Project description
  - Installation instructions
  - Usage guide
  - Example prompts
  - Screenshots/GIFs

- [ ] **10.2: Complete BRAINLIFT.md**
  - Daily logs
  - Key decisions
  - Challenges and solutions
  - AI prompts used
  - Learning breakthroughs

- [ ] **10.3: Code Comments**
  - Document public APIs
  - Explain complex logic
  - Add TODO for future work

- [ ] **10.4: Create Demo Script**
  - Introduction (30s)
  - Forked repo explanation (30s)
  - Feature walkthrough (2-3 min)
  - Technical architecture (1 min)
  - AI acceleration reflection (30s)

- [ ] **10.5: Record Demo Video**
  - 5 minutes max
  - Show multiple generation examples
  - Show error handling
  - Show undo/redo

- [ ] **10.6: Final Testing**
  - Fresh clone and build
  - Test all features
  - Verify documentation accuracy

**PR Checklist:**

- [ ] README is complete and accurate
- [ ] BRAINLIFT.md is comprehensive
- [ ] Code is well-commented
- [ ] Demo video is recorded
- [ ] Project builds from fresh clone

---

## MVP Completion Checklist

### Required Features:

- [ ] Dock panel in Godot editor
- [ ] Natural language input field
- [ ] Generate and Clear buttons
- [ ] Progress indication during generation
- [ ] Claude API integration
- [ ] JSON response parsing
- [ ] Node creation (CharacterBody2D, Sprite2D, Area2D, Control, etc.)
- [ ] Property application (position, scale, color)
- [ ] GDScript generation and attachment
- [ ] Placeholder sprites (colored rectangles)
- [ ] Collision shape creation
- [ ] Undo/Redo integration
- [ ] Error handling and display
- [ ] API key configuration

### Testing Scenarios:

- [ ] "Add a player with WASD controls" → Playable CharacterBody2D
- [ ] "Create a blue sprite at position 200, 300" → Sprite2D with blue modulate
- [ ] "Add an enemy that patrols left and right" → CharacterBody2D with patrol script
- [ ] "Create a coin collectible" → Area2D with pickup signal
- [ ] "Add a health bar UI" → ProgressBar Control
- [ ] Ctrl+Z after generation → All generated nodes removed
- [ ] Invalid API key → Clear error message
- [ ] Network timeout → Clear error message

### Performance Targets:

- [ ] Generation completes in <15 seconds for typical prompts
- [ ] No editor freeze during generation (progress shows)
- [ ] Generated scripts run without parse errors
- [ ] Undo/Redo works in single step

---

## Post-MVP: Phase 2 Planning

**Future PRs (After MVP Deadline):**

- PR #11: Async HTTP (non-blocking generation)
- PR #12: Streaming responses (live output)
- PR #13: Project asset awareness (textures, existing scripts)
- PR #14: 3D node support
- PR #15: Animation generation
- PR #16: Generation history and templates
- PR #17: Local LLM support (Ollama)
- PR #18: Windows/Linux builds