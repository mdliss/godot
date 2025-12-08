# Scene Builder Agent and Per‑Scene Prompt Strategy

The **Scene Builder Agent** consumes a game spec and produces:

- Natural language prompts for the NL Scene Builder editor dock (one per scene).
- A simple checklist of expected nodes and behaviours for each scene, which QA
  can later compare against the generated game.

This document defines the agent’s role, its inputs/outputs, and a sketch of the
prompt format it should follow.

---

## Role and Responsibilities

**Inputs:**

- Parsed game spec (from the Game Spec Agent), following the schema defined in
  `docs/game_spec_agent.md`.
- A target scene list (implicit in the spec’s `scenes[]` array).

**Outputs:**

For each scene in the spec:

1. **Scene generation prompt** (for the NL Scene Builder dock), written in
   natural language but grounded in the spec.
2. **Expectation checklist** describing what the agent thinks should appear in
   the scene after generation.

Artifacts (proposed):

- `docs/artifacts/scene_prompts/<game_slug>/<scene_id>.prompt.md`
- `docs/artifacts/scene_expectations/<game_slug>/<scene_id>.checklist.md`

---

## Scene Generation Prompt Content

Each scene prompt should:

- Start with a **concise scene purpose**:
  - Example: “Main menu with a title and a Start button that loads the game.”
- Reference relevant parts of the spec:
  - Player presence (if applicable).
  - Enemies/collectibles to place.
  - UI elements to display.
- Use terminology that maps well onto Godot 4 2D nodes:
  - `CharacterBody2D`, `Sprite2D`, `Area2D`, `CollisionShape2D`, `Camera2D`,
    `Label`, `Button`, `ProgressBar`, `Timer`, `AudioStreamPlayer2D`.
- Avoid inventing unsupported features (3D, tile painting logic, complex FX).

Example (high‑level, not binding):

> “Create a single 2D level scene for a top‑down arena shooter.  
>  The player is a `CharacterBody2D` in the center that moves with the
>  input actions ui_left/right/up/down and shoots projectiles with ui_accept.  
>  Add a rectangular arena boundary and spawn positions at the edges where
>  simple chaser enemies appear. Show the player’s health as hearts or a
>  simple `ProgressBar` at the top, and show the score as a `Label` in the
>  top‑right. Make visuals simple colored shapes.”

The exact prompt wording is up to the agent; this doc just constrains style and
content.

---

## Expectation Checklist Format

For each scene, the Scene Builder Agent also emits a concise checklist. This
checklist is not strict machine validation, but a human‑ and agent‑readable
summary of what “done” looks like for that scene.

Suggested fields:

- `scene_id`: matches `scenes[].id` from the spec.
- `summary`: 1–2 sentences.
- `required_nodes`: list of node names/types that must exist.
- `important_properties`: key properties that should be set (e.g. movement
  speed fields, health, UI text).
- `signals_or_interactions`: important interactions (e.g. body_entered,
  button pressed).
- `notes`: optional clarifications or stretch goals.

Represented as Markdown for now, a checklist file could look like:

```md
Scene: arena_level

Summary:
- Single‑room arena where the player fights waves of enemies.

Required nodes:
- Player: CharacterBody2D with a movement script.
- At least one enemy spawn system (e.g. Node2D + Timer) that creates enemies.
- ScoreLabel: Label anchored to top‑right.
- HealthBar: ProgressBar or a row of hearts at top‑left.

Important properties:
- Player.move_speed is set to a reasonable value (e.g. ~200).
- Enemy scripts decrease player health on contact and increase score when destroyed.

Signals/interactions:
- Enemy or projectile collision reduces health.
- On player death, emit a signal or call a function that transitions to game_over.

Notes:
- Place player roughly in the center of the arena.
- Keep visuals as simple colored rectangles for now.
```

The Scene Builder Agent doesn’t need to know the exact node paths; descriptive
names and types are enough for QA to reason about later.

---

## Agent Prompt Sketch

A conceptual prompt to drive the Scene Builder Agent might look like:

> **System**  
> You are a scene design assistant for a Godot 4 2D editor plugin.  
> Given a structured 2D game spec, you design per‑scene natural language
> prompts for a “NL Scene Builder” tool and a short checklist of what each
> generated scene should contain.  
> Use only 2D nodes and simple behaviours that the plugin can realistically
> generate.
>
> **User**  
> 1. Here is the game spec (JSON):  
>    `<GAME_SPEC_JSON>`  
> 2. For each scene in `scenes[]`, produce:  
>    - A natural language prompt for that scene (for an NL Scene Builder dock
>      inside the Godot editor).  
>    - A Markdown checklist describing expected nodes and behaviours.  
> 3. Constraints:  
>    - Only use 2D Godot nodes and GDScript‑level behaviours.  
>    - Prefer a few key elements over a huge, cluttered scene.  
>    - Keep each prompt short enough to be readable in an editor dock.
>
> **Output format (conceptual)**  
> For each scene, respond with a JSON object containing:  
> `{ "scene_id": "...", "prompt": "...", "checklist_markdown": "..." }`.

The exact RPC/serialization format will be decided by the orchestrator, but the
intent is clear: one prompt + one checklist per scene.

---

## Example (Sketch Only)

Given the “Top‑Down Shooter: Waves” spec:

- Scene `main_menu`:
  - Prompt: title label + Start button that loads the arena.
  - Checklist: presence of title `Label`, `Button`, and signal hookup.
- Scene `arena_level`:
  - Prompt: player in center, enemy spawners, simple score/health UI.
  - Checklist: player node, enemy spawners, projectiles, score/health UI.
- Scene `game_over`:
  - Prompt: show final score and Retry button.
  - Checklist: score label, Retry button, signal to reload arena or main menu.

The detailed wording and ordering is left to the Scene Builder Agent and
orchestrator prompts.

