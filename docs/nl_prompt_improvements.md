# NL Prompt Pipeline – Improvement Plan

The current natural-language → scene pipeline does the bare minimum: we paste a free-form prompt into the dock, hope the system prompt in `system_prompt.h` steers the model, and accept whatever JSON comes back. That works for toy drafts, but it breaks down as soon as we want richer sprites, scripted interactions, and multi-scene continuity. This note captures practical upgrades you can layer on without rebuilding the world.

## Pain Points Today

- **Flat prompts.** The dock only receives raw text, so the LLM guesses missing details (camera, assets, signal hookups) and often invents conflicting nodes.
- **No notion of context.** We never tell the model which scene is open, which assets already exist, or what the rest of the spec expects.
- **Single-pass generation.** Layout, scripts, signals, and metadata are all requested in one response, so any parsing failure loses the entire attempt.
- **Weak validation feedback.** ResponseParser just skips invalid nodes; the user receives vague warnings instead of actionable “regenerate X with Y constraint.”

## Goals for the Next Iterations

1. Prompts should be **structured capsules** that encode scene goals, must-have entities, assets to reuse, and checklists the model must satisfy.
2. The dock should automatically **inject scene context** (node summaries, QA checklist gaps, selected assets) rather than relying on the user to restate them manually.
3. Generation should move from a single shot to a **multi-pass flow** (layout → scripts → signals/UIs) with validation between each pass.
4. After each pass, the tool should provide **machine-readable deltas** back to the orchestrator so QA/Test agents can reason about coverage.

## Concrete Improvements

### 1. Context Packets from the Orchestrator
- Reuse `docs/nl_game_orchestrator.md` to produce a JSON “scene brief” for each prompt containing:
  - Spec excerpt (core loop, entities, win/lose conditions).
  - Scene-specific checklist from `docs/scene_builder_agent.md`.
  - Asset catalog (sprites, tiles, prefabs) discovered in the project.
  - Current scene summary (node tree + scripts) so the model knows what already exists.
- Have the CLI write these packets to `docs/artifacts/scene_prompts/<slug>/<scene_id>.json` and let the dock load them via file picker or slug dropdown.

### 2. Layered Prompt Template
- Replace the free-text prompt with a form that builds a message like:

  ```
  ### SCENE GOAL
  ...
  ### REQUIRED ELEMENTS
  - Player: CharacterBody2D with stick-figure visual + movement script template A
  - Enemy: Follow-player script template B
  - UI: CanvasLayer/HUD/ScoreLabel
  ### RESOURCE HINTS
  - texture://...
  - prefab://...
  ### CONSTRAINTS
  - Keep to <= 15 nodes
  - Reuse existing Player node if present
  ### CHECKLIST
  1. ...
  ```

- Keep the system prompt general and let this structured “user” prompt drive specifics. This makes it simple to evolve the recipe without recompiling.

### 3. Multi-Pass Generation Strategy
1. **Layout pass** – ask the LLM only for `nodes` (no scripts). Validate parent references via the new `_has_available_parent()` helper, auto-retry missing parents with hints.
2. **Script pass** – feed the accepted node list back to the model with “attach scripts for nodes A/B, keep to template X.” Parse and attach scripts separately so script failures don’t nuke the layout.
3. **Signals & polish pass** – request signal definitions and simple UI updates once nodes+scripts succeed.

Each pass can reuse the same HTTP call infrastructure but with different “mode” tags in the prompt. This dramatically reduces JSON parsing failures and surfaces better error messages (“script compilation failed, rerunning script pass only”).

### 4. Validator-Driven Feedback
- Extend `ResponseParser` to score outputs against the checklist and emit a machine-readable verdict (missing player script, missing HUD, etc.).
- When the dock detects a violation, show targeted actions: “Regenerate scripts for Player + Enemy,” “Add Camera2D under Player,” etc.
- Feed those same verdicts back into the orchestrator so the QA agent can decide whether to re-run prompts or escalate to the user.

### 5. Asset- & Prefab-Aware Prompts
- Generate a lightweight asset manifest (textures, PackedScenes, shape resources) at project load and expose it to the LLM as `ASSET_LIBRARY` in the structured prompt.
- Encourage the model to reference assets via resource paths (the runtime now loads them thanks to the `_create_resource` work), reducing anonymous ColorRects.
- Provide a small “prefab dictionary” (e.g., `PlayerStickFigure`, `EnemyDrone`, `HUDPanel`) that the model can instance via shorthand instructions.

### 6. Auto-Summarize Results
- After generation, run a cheap scene summarizer (node names, attached scripts, signals) and store it next to the scene prompt artifacts. This summary feeds QA, but it also gives the next prompt run concrete context (“Player already has movement script; only add ranged attack”).

### 7. Tooling Hooks
- Add a “Prompt Builder” tab to the dock that lets users toggle requirements (e.g., “Add co-op second player”, “Add score UI”). The dock can then synthesize the structured prompt without hand-editing markdown.
- Surface a “Regenerate Part” menu (Layout/Scripts/Signals) that simply replays the relevant pass with the last structured prompt.

## Example Enhanced Prompt Capsule

```
SCENE_ID: level_1
SCENE_GOAL:
  Recreate the windy cliff level. Player must climb platforms, dodge gust-driven orbs, and reach the summit flag.
REQUIRED_ELEMENTS:
  - Player (CharacterBody2D) + StickFigure prefab + movement template A
  - WindOrb (RigidBody2D) prefab w/ gust impulse script
  - SummitFlag (Area2D) w/ goal script template G
  - Camera2D child of Player, smoothing enabled
RESOURCE_HINTS:
  - texture: res://art/player.png
  - prefab: res://prefabs/wind_orb.tscn
EXISTING_SCENE_CONTEXT:
  - Player node exists (keep transforms, replace script)
  - Ground + two platforms already present
CHECKLIST:
  1. Player keeps collision + camera + new script
  2. At least 3 WindOrb instances at varying heights
  3. SummitFlag near y=150 with goal script
CONSTRAINTS:
  - <= 25 top-level nodes
  - Scripts must avoid autoloads
PASS: layout
```

Follow with script and signal passes that reference the same capsule but change `PASS` and the per-pass instructions.

## Rollout Path

1. **Week 1** – Build the prompt capsule generator inside `tools/nl_game_pipeline` and teach the dock to load/show it.
2. **Week 2** – Implement multi-pass HTTP calls + validator feedback loop in `LLMService`/`NLInputPanel`/`SceneGenerator`.
3. **Week 3** – Produce asset manifests + prefab dictionary; update the system prompt with guidance on using them.
4. **Week 4** – Add scene summarizer + QA feedback loop so failures automatically produce new prompt capsules.

Each milestone is independent, so you can roll improvements gradually and keep the current single-prompt mode as a fallback.
