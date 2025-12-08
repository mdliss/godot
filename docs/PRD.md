# Godot NL Scene Builder - Product Requirements Document

**Project**: Godot NL Scene Builder - Natural Language to 2D Scene Generation  
**Goal**: Enable game developers to describe scenes in plain English and have them automatically generated in the Godot editor  
**Base Repository**: https://github.com/godotengine/godot (Fork)

**Note**: This MVP focuses on 2D scene generation with common node types. 3D support, custom asset integration, and advanced scripting patterns are planned for future phases.

---

## Core Architecture (MVP)

**Brownfield C++ Modification:**

- MVP modifies Godot's `editor/` source code directly
- Adds a new editor panel for natural language input
- Uses Godot's built-in `HTTPClient` for Claude API calls
- Leverages existing scene tree manipulation APIs
- Generates GDScript for node behaviors

**Integration Points:**

- Custom EditorPlugin in C++
- New dock panel in editor UI
- Scene tree manipulation via `EditorInterface`
- Script generation to attach to nodes

---

## User Stories

### Primary User: Game Developer (MVP Priority)

- As a game developer, I want to **describe a scene in plain English** so that I can rapidly prototype without manual node creation
- As a game developer, I want to **see a preview of what will be generated** so that I can confirm before committing changes
- As a game developer, I want to **generate player characters with movement controls** so that I have a playable prototype immediately
- As a game developer, I want to **create enemies with basic AI behaviors** so that I can test gameplay interactions
- As a game developer, I want to **add UI elements like health bars and score displays** so that I have a complete game loop
- As a game developer, I want to **undo generated scenes** so that I can iterate quickly without manual cleanup

### Secondary User: Beginner/Learner (Implement After Primary)

- As a beginner, I want to **see the generated GDScript code** so that I can learn how to implement features manually
- As a beginner, I want to **get explanations of what was generated** so that I understand the node hierarchy

---

## Key Features for MVP

### 1. NL Input Panel

**Must Have:**

- Dockable panel in Godot editor (similar to Inspector/FileSystem docks)
- Multi-line text input field for natural language prompts
- "Generate" button to trigger scene creation
- "Clear" button to reset input
- Status indicator showing generation progress
- Error display for failed generations

**Input Examples:**

- "Add a player with WASD controls"
- "Create an enemy that patrols left and right"
- "Add a coin collectible that disappears when touched"
- "Create a health bar UI in the top left"
- "Add a tilemap ground platform"

**Success Criteria:**

- Panel docks correctly in Godot editor
- Text input accepts multi-line prompts
- Clear visual feedback during generation (spinner/progress)
- Errors display with actionable messages

### 2. LLM Integration (Claude API)

**Must Have:**

- HTTP POST requests to Claude API from C++
- Structured JSON output parsing
- System prompt optimized for Godot scene generation
- Context about current scene state (existing nodes)
- Token usage tracking (display in panel)
- API key configuration (editor settings or environment variable)

**Request Structure:**

```
System: You are a Godot 4 scene generation assistant. Output valid JSON...
User: [Current scene context] + [User's natural language request]
Response: { "nodes": [...], "scripts": [...], "connections": [...] }
```

**Response Schema:**

```json
{
  "nodes": [
    {
      "name": "Player",
      "type": "CharacterBody2D",
      "parent": "root",
      "properties": {
        "position": { "x": 100, "y": 200 }
      },
      "children": [
        {
          "name": "Sprite2D",
          "type": "Sprite2D",
          "properties": {
            "texture": "placeholder",
            "modulate": "#3498db"
          }
        },
        {
          "name": "CollisionShape2D",
          "type": "CollisionShape2D",
          "properties": {
            "shape": "RectangleShape2D",
            "size": { "x": 32, "y": 32 }
          }
        }
      ]
    }
  ],
  "scripts": [
    {
      "attach_to": "Player",
      "code": "extends CharacterBody2D\n\nvar speed = 200.0\n\nfunc _physics_process(delta):\n\tvar velocity = Vector2.ZERO\n\tif Input.is_action_pressed(\"ui_right\"):\n\t\tvelocity.x += 1\n\tif Input.is_action_pressed(\"ui_left\"):\n\t\tvelocity.x -= 1\n\tif Input.is_action_pressed(\"ui_down\"):\n\t\tvelocity.y += 1\n\tif Input.is_action_pressed(\"ui_up\"):\n\t\tvelocity.y -= 1\n\tvelocity = velocity.normalized() * speed\n\tmove_and_slide()\n"
    }
  ],
  "signals": []
}
```

**Success Criteria:**

- API calls complete within 10 seconds for typical requests
- JSON parsing handles malformed responses gracefully
- API errors display user-friendly messages
- Works with Claude claude-sonnet-4-20250514 model

### 3. Scene Generation Engine

**Must Have:**

- Parse LLM JSON response into Godot nodes
- Create node hierarchy in current scene
- Set node properties (position, scale, rotation, modulate)
- Attach generated GDScript to nodes
- Support for common 2D node types:
  - CharacterBody2D, RigidBody2D, StaticBody2D, Area2D
  - Sprite2D, AnimatedSprite2D
  - CollisionShape2D, CollisionPolygon2D
  - Camera2D
  - TileMap, TileMapLayer
  - Control, Label, Button, ProgressBar, TextureRect
  - Timer, AudioStreamPlayer2D
- Placeholder sprites (colored rectangles) when no texture specified
- Default collision shapes based on sprite size

**Node Creation Flow:**

1. Parse JSON response
2. Validate node types exist in Godot
3. Create nodes in scene tree
4. Set properties via `set()` API
5. Generate and attach scripts
6. Select newly created nodes in editor

**Success Criteria:**

- All supported node types create correctly
- Properties apply without errors
- Scripts attach and are syntactically valid
- Undo (Ctrl+Z) reverts entire generation
- Generated nodes appear in scene tree immediately

### 4. GDScript Generation

**Must Have:**

- Generate valid Godot 4 GDScript syntax
- Common patterns:
  - WASD/Arrow key movement (CharacterBody2D)
  - Patrol AI (move between points)
  - Collectible pickup (Area2D signals)
  - Health/damage systems
  - Score tracking
  - Scene transitions
- Proper indentation (tabs, not spaces)
- Signal connections where appropriate
- Export variables for tweakable parameters

**Script Templates (embedded in system prompt):**

```gdscript
# Player Movement Template
extends CharacterBody2D

@export var speed: float = 200.0

func _physics_process(delta):
    var input_vector = Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down")
    velocity = input_vector * speed
    move_and_slide()
```

**Success Criteria:**

- Generated scripts run without parse errors
- Movement scripts work with default input mappings
- Collision/signal scripts connect properly
- Export variables appear in Inspector

### 5. Editor Integration

**Must Have:**

- Panel position saves between editor sessions
- Keyboard shortcut to focus NL input (Ctrl+Shift+N)
- Generation respects current scene (adds to, doesn't replace)
- Works in both 2D and 3D editor views (generates 2D nodes)
- Undo/Redo integration via EditorUndoRedoManager

**Editor Settings:**

- `nl_scene_builder/api_key`: Claude API key (password field)
- `nl_scene_builder/model`: Model selection (default: claude-sonnet-4-20250514)
- `nl_scene_builder/max_tokens`: Max response tokens (default: 4096)

**Success Criteria:**

- Settings persist in editor_settings
- Panel state persists across editor restarts
- Undo works for entire generation batch
- No editor crashes or freezes during generation

### 6. Error Handling & Feedback

**Must Have:**

- Network error handling (timeout, connection failed)
- API error handling (rate limit, invalid key, server error)
- JSON parse error handling (malformed response)
- Node creation error handling (invalid type, invalid property)
- Progress indication during API call
- Success message with node count after generation

**Error Messages:**

| Error Type | User Message |
|------------|--------------|
| No API key | "Please set your Claude API key in Editor Settings > NL Scene Builder" |
| Network timeout | "Request timed out. Check your internet connection." |
| Rate limited | "API rate limit reached. Please wait a moment." |
| Invalid JSON | "Failed to parse AI response. Try rephrasing your request." |
| Unknown node type | "Skipped unknown node type: [type]. Other nodes created successfully." |

**Success Criteria:**

- No crashes on any error condition
- All errors display in panel (not just console)
- Partial success still creates valid nodes
- User can retry after errors

---

## Data Model

### Editor Settings Storage

```cpp
// Stored in EditorSettings
nl_scene_builder/api_key         // String (encrypted)
nl_scene_builder/model           // String
nl_scene_builder/max_tokens      // int
nl_scene_builder/panel_position  // int (dock slot)
```

### Generation Request (Internal)

```cpp
struct NLGenerationRequest {
    String prompt;                    // User's natural language input
    String scene_context;             // JSON of current scene structure
    int max_tokens;                   // From settings
};
```

### Generation Response (From LLM)

```cpp
struct NLGenerationResponse {
    Vector<NodeDefinition> nodes;     // Nodes to create
    Vector<ScriptDefinition> scripts; // Scripts to attach
    Vector<SignalConnection> signals; // Signals to connect
    String explanation;               // Optional: explain what was generated
};

struct NodeDefinition {
    String name;
    String type;
    String parent;                    // Node path or "root"
    Dictionary properties;            // Property name -> value
    Vector<NodeDefinition> children;
};

struct ScriptDefinition {
    String attach_to;                 // Node path
    String code;                      // GDScript source
};

struct SignalConnection {
    String source_node;
    String signal_name;
    String target_node;
    String method_name;
};
```

---

## Tech Stack

### Core (Dictated by Godot)

- **Language**: C++17
- **Build System**: SCons
- **Platform**: macOS (arm64), extensible to Windows/Linux

### Integration

- **LLM**: Claude API (claude-sonnet-4-20250514)
- **HTTP**: Godot's built-in `HTTPClient` class
- **JSON**: Godot's built-in `JSON` class
- **UI**: Godot's editor UI framework (Control nodes)

### Key Godot Classes to Use

| Class | Purpose |
|-------|---------|
| `EditorPlugin` | Main plugin class, registers dock |
| `EditorInterface` | Access to editor state, scene tree |
| `EditorUndoRedoManager` | Undo/redo integration |
| `HTTPClient` | HTTP requests to Claude API |
| `JSON` | Parse API responses |
| `Control` | Base for UI panel |
| `TextEdit` | Multi-line input |
| `Button` | Generate/Clear buttons |
| `RichTextLabel` | Status/output display |

---

## Out of Scope for MVP

### Features NOT Included:

- 3D scene generation (3D nodes, meshes, materials)
- Custom asset import (textures, sounds, models)
- Animation generation (AnimationPlayer keyframes)
- Shader generation
- Tilemap painting (only TileMap node creation)
- Multi-scene generation
- Project-wide refactoring
- Version control integration
- Collaborative editing
- Voice input

### Technical Items NOT Included:

- Streaming API responses (wait for complete response)
- Local LLM support (Ollama, llama.cpp)
- Fine-tuned models
- Prompt caching/history
- Generation templates/presets
- Plugin marketplace distribution
- Windows/Linux builds (macOS only for MVP)
- Automated testing framework
- Localization/i18n

---

## Known Limitations & Trade-offs

1. **Placeholder graphics**: Generated sprites are colored rectangles, not real assets (user must replace)
2. **No asset awareness**: LLM doesn't know what assets exist in project (future: context injection)
3. **Single scene**: Generates in current scene only, no cross-scene references
4. **Synchronous API calls**: Editor may feel unresponsive during generation (future: async with callback)
5. **English only**: Prompts must be in English (LLM limitation)
6. **GDScript only**: No C#, VisualScript, or GDExtension generation
7. **Limited node types**: Only common 2D nodes supported (expandable)
8. **No animation**: Static scenes only, no keyframe generation

---

## Success Metrics for MVP

1. **"Add a player with WASD controls"** generates a playable CharacterBody2D in <10 seconds
2. **"Create an enemy that patrols"** generates working patrol AI
3. **"Add a coin collectible"** generates Area2D with pickup signal
4. **"Add a health bar"** generates functional ProgressBar UI
5. **Undo (Ctrl+Z)** reverts entire generation in one step
6. **API errors** display clearly in panel without crashing editor
7. **Generated scripts** run without parse errors

---

## MVP Testing Checklist

### Core Functionality:

- [ ] Panel appears in editor dock
- [ ] Text input accepts multi-line prompts
- [ ] Generate button triggers API call
- [ ] Progress indicator shows during generation
- [ ] Nodes appear in scene tree after generation

### Node Generation:

- [ ] CharacterBody2D creates with collision
- [ ] Sprite2D creates with placeholder color
- [ ] Area2D creates with collision shape
- [ ] Control nodes create (Label, Button, ProgressBar)
- [ ] Node properties apply correctly (position, scale, modulate)
- [ ] Child nodes create in correct hierarchy

### Script Generation:

- [ ] Scripts attach to correct nodes
- [ ] Scripts have valid GDScript syntax
- [ ] Movement scripts work with keyboard input
- [ ] Signal connections work (Area2D body_entered)
- [ ] Export variables appear in Inspector

### Error Handling:

- [ ] Missing API key shows helpful message
- [ ] Network timeout handled gracefully
- [ ] Invalid JSON response doesn't crash
- [ ] Unknown node type skipped with warning
- [ ] Partial generation still creates valid nodes

### Editor Integration:

- [ ] Undo reverts entire generation
- [ ] Panel position persists across restarts
- [ ] Settings save in EditorSettings
- [ ] No memory leaks after repeated generations

---

## Risk Mitigation

**Biggest Risk:** LLM generates invalid node types or properties  
**Mitigation:** Validate all node types against ClassDB before creation; skip invalid nodes with warning; comprehensive system prompt with examples

**Second Risk:** API latency makes editor feel frozen  
**Mitigation:** Show clear progress indicator; consider async HTTP in future; set reasonable timeout (30s)

**Third Risk:** Generated GDScript has syntax errors  
**Mitigation:** Include working code examples in system prompt; validate script compilation before attaching; show error if script invalid

**Fourth Risk:** Undo doesn't work correctly  
**Mitigation:** Use EditorUndoRedoManager; batch all node creations in single undo action; test thoroughly

**Fifth Risk:** Build system complexity (SCons + C++)  
**Mitigation:** Start with minimal changes; follow Godot contribution guidelines; incremental compilation with `-j10`