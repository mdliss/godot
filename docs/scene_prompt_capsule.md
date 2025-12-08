# Scene Prompt Capsule Schema

The **Scene Prompt Capsule** is a JSON sidecar that travels with every
`docs/artifacts/scene_prompts/<slug>/<scene_id>` artifact set. Capsules carry
all the structured context the NL Scene Builder dock needs to assemble rich
LLM prompts (scene goals, required elements, asset hints, checklists, etc.).

## File Naming

```
docs/artifacts/scene_prompts/<slug>/<scene_id>.capsule.json
```

Each capsule lives next to the existing Markdown prompt/checklist files for the
same scene.

## JSON Structure (v1)

```jsonc
{
  "capsule_version": 1,
  "slug": "string",
  "scene_id": "string",
  "idea": "raw idea text",
  "scene_goal": "short human summary",
  "prompt_md": "markdown NL prompt",
  "checklist_md": "markdown checklist",
  "required_elements": ["Player", "HUD", ...],
  "resource_hints": [
    {"type": "texture", "path": "res://art/player.png", "notes": "Optional"}
  ],
  "constraints": ["<= 25 nodes", "Reuse Player if exists"],
  "llm_passes": ["layout"],
  "existing_context": {
    "spec_excerpt": {
      "title": "...",
      "core_loop": "..."
    },
    "notes": ["Any reminders"],
    "scene_summary": "Optional summary of the current scene"
  }
}
```

Notes:

- `prompt_md` and `checklist_md` are the exact Markdown files we already
  generate; storing them here lets the dock rebuild a structured “capsule
  prompt” without scraping the filesystem twice.
- `required_elements` is a simple list extracted from the checklist (bullets
  that translate into concrete nodes or behaviours). The dock can surface these
  to the user and ensure they make it into the LLM request.
- `resource_hints` and `constraints` are optional arrays. Leave them empty when
  nothing special is needed.
- `llm_passes` tracks which generation passes the capsule is valid for
  (`layout`, `scripts`, `signals`, etc.). For v1 we only emit `"layout"` but the
  dock uses the array to label the UI and future-proof the format.

## Example Capsule

```json
{
  "capsule_version": 1,
  "slug": "windy_cliffs",
  "scene_id": "level_1",
  "idea": "A windy cliffside climb with gusts knocking the player back.",
  "scene_goal": "Player climbs staggered platforms, dodging wind orbs to reach the summit flag.",
  "prompt_md": "# NL Scene Builder Prompt...",
  "checklist_md": "Scene: level_1\n\nSummary...",
  "required_elements": [
    "Player: CharacterBody2D with stick figure visual + movement script",
    "WindOrb: RigidBody2D gust hazard (x3)",
    "SummitFlag: Area2D win trigger",
    "HUD: Health + Score labels"
  ],
  "resource_hints": [
    {"type": "texture", "path": "res://art/player.png"},
    {"type": "prefab", "path": "res://prefabs/wind_orb.tscn"}
  ],
  "constraints": ["Keep under 25 nodes", "Reuse existing Player node if present"],
  "llm_passes": ["layout"],
  "existing_context": {
    "spec_excerpt": {
      "title": "Windy Cliffs",
      "core_loop": "Climb, dodge gusts, capture the flag."
    },
    "notes": ["Level already has Ground + Platforms nodes"],
    "scene_summary": "Player, Ground, PlatformA, PlatformB currently in scene."
  }
}
```

## Usage Expectations

- **CLI / Orchestrator**: whenever prompts are generated (either via OpenAI or
  stubbed markdown), emit the capsule JSON alongside the Markdown files.
- **NL Scene Builder Dock**: provide a “Load Capsule…” action. When a capsule is
  selected, the dock should surface the metadata (scene goal, requirements) and
  auto-build a structured user prompt for the LLM call.
- **QA / Automation**: a future scene summarizer can update `existing_context`
  after each generation pass so downstream agents know what’s actually in the
  scene.

This schema is intentionally flexible—unknown fields should be ignored so we
can extend it without breaking older builds.
