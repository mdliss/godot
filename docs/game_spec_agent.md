# Game Spec Agent and 2D Game Spec Schema

The **Game Spec Agent** takes a short natural language idea and turns it into a
structured 2D game specification that other agents (Scene Builder, QA Tester)
and tools (NL Scene Builder plugin) can consume.

This document defines:

- The Game Spec Agent’s role.
- The game spec schema (fields, constraints).
- A prompt template for the agent.
- A small worked example.

The schema is intentionally compact and constrained to what the NL Scene Builder
and Godot 4 can reasonably generate for 2D MVPs.

---

## Role and Responsibilities

**Input:**

- A short, free‑form game idea text, e.g.:
  - `"a simple top‑down shooter with waves of enemies and a score counter"`

**Output:**

- A machine‑readable game spec object following the schema below, typically
  serialized as JSON and saved under:
  - `docs/artifacts/game_specs/<game_slug>.json`

**Responsibilities:**

- Normalize vague text into specific, implementable decisions:
  - Player movement model, win/lose conditions, basic enemy behaviours.
- Stay within **2D Godot 4** and **GDScript** capabilities:
  - Use only common node types (e.g. `CharacterBody2D`, `Area2D`, `Sprite2D`,
    `Label`, `ProgressBar`, `Timer`).
  - No 3D, advanced animation, custom shaders, or asset‑aware behaviours.

---

## Game Spec Schema (v0)

Represented here as JSON for clarity; the agent should emit a JSON object
matching this structure.

```json
{
  "id": "top_down_shooter_waves",
  "title": "Top-Down Shooter: Waves",
  "genre": "top_down_shooter",
  "core_loop": "Move around the arena, shoot incoming enemies, survive waves to increase score.",
  "camera": {
    "type": "top_down",
    "follows_player": true
  },
  "player": {
    "name": "Player",
    "description": "A ship that can move freely and shoot projectiles.",
    "body_type": "CharacterBody2D",
    "controls": [
      "move with WASD or arrow keys",
      "shoot with Space"
    ],
    "stats": {
      "max_health": 3,
      "move_speed": 200
    }
  },
  "enemies": [
    {
      "id": "basic_chaser",
      "name": "Chaser",
      "description": "Slow enemy that moves toward the player.",
      "behaviour": "spawn at arena edges and move directly at the player",
      "stats": {
        "health": 1,
        "move_speed": 120,
        "damage_on_contact": 1
      }
    }
  ],
  "collectibles": [],
  "ui": {
    "show_health": true,
    "show_score": true,
    "other_elements": []
  },
  "scenes": [
    {
      "id": "main_menu",
      "type": "menu",
      "description": "Simple start screen with a title and a Start button.",
      "entry_point": true
    },
    {
      "id": "arena_level",
      "type": "level",
      "description": "Single arena where player fights waves of enemies.",
      "entry_point": false
    },
    {
      "id": "game_over",
      "type": "screen",
      "description": "Shows final score and a retry button.",
      "entry_point": false
    }
  ],
  "win_condition": "Survive all waves (or reach a target score).",
  "lose_condition": "Player health reaches 0.",
  "notes": [
    "Keep visuals simple: colored rectangles and basic shapes.",
    "Prefer Godot input actions: ui_left, ui_right, ui_up, ui_down, ui_accept."
  ]
}
```

### Field Guidelines

- `id`: short slug, lower_snake_case, used in filenames.
- `title`: human‑readable game title.
- `genre`: one of a small set (e.g. `"top_down_shooter"`, `"platformer"`,
  `"arena_survival"`, `"endless_runner"`, `"puzzle"`).
- `core_loop`: 1–3 sentences describing what the player repeatedly does.
- `camera`:
  - `type`: `"top_down"` or `"side_view"` for now.
  - `follows_player`: boolean.
- `player`:
  - `body_type` must be a supported 2D body node, usually `CharacterBody2D`.
  - `controls` should refer to Godot input actions where possible.
- `enemies[]`:
  - Keep behaviours simple and describable in a sentence or two.
- `ui`:
  - Flags for health/score plus a list of any other key elements.
- `scenes[]`:
  - 2–5 scenes per small game is typical.
  - `entry_point` should be `true` for exactly one scene (usually main menu or
    first level).
- `notes[]`:
  - Anything helpful to downstream agents, such as tone, difficulty, or
    technical hints.

---

## Prompt Template for the Game Spec Agent

This is a *conceptual* template the orchestrator or Codex CLI would use when
calling the Game Spec Agent.

> **System (summary)**  
> You are a game design assistant specialized in small 2D Godot 4 games.  
> You convert short natural language ideas into structured 2D game specs that
> follow a given JSON schema. Your designs must be feasible to implement with
> common 2D nodes, GDScript, and simple placeholder art. No 3D, no complex
> shaders, and no features outside the provided schema.
>
> **User (schema + idea)**  
> 1. Here is the JSON schema you must follow (field names and types):  
>    [include brief version of schema above]  
> 2. Here is the game idea:  
>    `<RAW_GAME_IDEA_TEXT>`  
> 3. Requirements:  
>    - Choose 2–5 scenes.  
>    - Keep enemy and UI behaviours simple.  
>    - Only use node types and concepts that a 2D Godot 4 editor plugin could
>      generate (e.g., CharacterBody2D, Area2D, Sprite2D, Label, ProgressBar).  
>    - Assume input actions: ui_left, ui_right, ui_up, ui_down, ui_accept,
>      ui_cancel.  
>
> **Output:**  
> Respond with **only** a single JSON object that conforms to the schema.

Downstream tasks can refine this template and inject more constraints (e.g.
token budgets, difficulty settings).

---

## Worked Example (Sketch)

Input idea:

> `"A tiny 2D platformer where the player collects 10 coins and avoids spikes."`

The Game Spec Agent might output a spec with:

- `genre`: `"platformer"`.
- `player.body_type`: `"CharacterBody2D"`.
- One main level scene with platforms, coins as `Area2D` pickups, and spikes as
  damaging hazards.
- UI: score label and maybe a remaining‑coins label.
- Win: collect all coins.  
  Lose: touch spikes or fall off the level.

The exact JSON is left to the agent, but should always match the schema and
stay within 2D + GDScript constraints.

