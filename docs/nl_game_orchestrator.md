# NL Game Orchestrator – One‑Shot 2D Game Pipeline

The **NL Game Orchestrator** is a small, scriptable entry point that chains
the multi‑agent pipeline into a single flow:

> raw natural language idea → Game Spec Agent → Scene Builder Agent  
> → Godot NL Scene Builder → QA Tester Agent → reports

This document sketches the orchestrator design so we can later implement it as
one or more CLI tools and/or Task Master workflows.

---

## Goals

- Take a single, short game idea as input.
- Run a sequence of agents to:
  1. Produce a structured 2D game spec.
  2. Produce per‑scene prompts + expectations.
  3. (Optionally) assist in driving the Godot NL Scene Builder dock.
  4. Produce a QA report and test plan.
- Save all intermediate artifacts in predictable locations under `docs/`.
- Be automatable via Codex CLI / Task Master without GUI prompts.

Out of scope (for now):

- Fully automating the Godot editor UI.
- Running actual in‑engine tests or gameplay simulations.

---

## High‑Level Flow

1. **Input and setup**
   - User calls the orchestrator CLI with a single raw idea string and an
     optional slug, e.g.:
     - `nl-game new "simple top-down shooter" --slug shooter01`

2. **Game Spec Agent**
   - Orchestrator calls the Game Spec Agent (see `docs/game_spec_agent.md`)
     with the raw idea.
   - Receives a JSON game spec and writes it to:
     - `docs/artifacts/game_specs/<slug>.json`

3. **Scene Builder Agent**
   - Orchestrator reads the game spec and calls the Scene Builder Agent
     (see `docs/scene_builder_agent.md`).
   - Receives a set of per‑scene prompts + checklists.
   - Writes them to:
     - `docs/artifacts/scene_prompts/<slug>/<scene_id>.prompt.md`
     - `docs/artifacts/scene_expectations/<slug>/<scene_id>.checklist.md`

4. **Godot NL Scene Builder (manual or semi‑automated)**
   - At this stage, the orchestrator can:
     - Print instructions telling the user which prompts to paste into the NL
       Scene Builder dock and for which scenes, **or**
     - (future work) call a scriptable interface that sends prompts into the
       dock automatically.
   - The user (or later automation) uses these prompts to generate/update
     scenes inside the Godot editor.

5. **Scene summaries**
   - Once scenes exist, a small helper script (future work) can scan the Godot
     project and emit lightweight scene summaries, such as:
     - Node tree structures.
     - Script attachments.
     - Basic signal connections.
   - These summaries would be written to:
     - `docs/artifacts/scene_summaries/<slug>/<scene_id>.json`

6. **QA Tester Agent**
   - Orchestrator calls the QA Tester Agent (see `docs/qa_tester_agent.md`)
     with:
     - The game spec JSON.
     - The per‑scene expectation checklists.
     - The scene summaries.
   - Receives a QA report + test plan and writes it to:
     - `docs/artifacts/qa_reports/<slug>.md`

7. **Final output**
   - CLI prints a short summary:
     - Paths to the spec, prompts, expectations, and QA report.
     - Any major issues flagged by QA.

---

## CLI Shape (Conceptual)

Exact implementation language is open (Python, Node.js, small Go/Rust tool),
but the interface should be simple. Examples:

```bash
# Create a new one-shot 2D game pipeline run
nl-game new "simple top-down shooter with score" --slug shooter01

# Re-run just the QA step after manual edits in Godot
nl-game qa --slug shooter01
```

Core commands (sketch):

- `nl-game new "<idea>" --slug <slug>`  
  End‑to‑end pipeline: spec → scene prompts/checklists → (manual Godot step)
  → QA.

- `nl-game spec "<idea>" --slug <slug>`  
  Run only the Game Spec Agent and write the spec.

- `nl-game prompts --slug <slug>`  
  Run only the Scene Builder Agent using an existing spec.

- `nl-game qa --slug <slug>`  
  Run the QA Tester Agent for an existing spec + expectations + summaries.

The orchestrator can be wired to Task Master tasks so that completing certain
tasks triggers parts of this flow.

---

## Integration with Agents

The orchestrator does not implement game design logic itself; instead it:

- **Calls Game Spec Agent**  
  - Passes the raw idea and schema instructions.  
  - Validates that the response is valid JSON conforming (loosely) to the
    schema.

- **Calls Scene Builder Agent**  
  - Supplies the game spec JSON.  
  - Validates that outputs contain `scene_id`, `prompt`, and
    `checklist_markdown` for each scene.

- **Calls QA Tester Agent**  
  - Supplies game spec JSON, expectation checklists, and scene summaries.  
  - Receives a Markdown QA report and minimal test plan.

In practice, these “calls” would be implemented as Codex CLI prompts or MCP
tool invocations, but the orchestrator design is agnostic to the exact
transport.

---

## Error Handling and Cross‑Verification (Conceptual)

The orchestrator should handle common failure modes:

- **Spec too vague or inconsistent**
  - Detect obvious schema violations or missing fields.
  - Optionally, re‑prompt the Game Spec Agent with clarifications.

- **Scene prompts drift from spec**
  - Compare Scene Builder outputs with the spec (light sanity checks).
  - Log warnings if, for example, a specified enemy type never appears in any
    scene prompts.

- **QA finds critical gaps**
  - If QA report marks certain scenes as critically incomplete (e.g. “no
    player node” or “no win condition”), the orchestrator can:
    - Surface those issues prominently to the user.
    - Optionally offer to regenerate prompts for affected scenes.

These loops can be introduced gradually; the initial implementation can simply
stop on errors and print actionable messages.

---

## Implementation Notes (Future Work)

- **Location in repo**
  - A natural place for the script(s) is:
    - `tools/nl_game_pipeline/` (e.g., `orchestrator.py` or `cli.ts`).

- **Config**
  - The orchestrator should read minimal configuration from:
    - `.env` (LLM keys, etc., already present).
    - Possibly a small config file under `.taskmaster/` for defaults.

- **Godot integration**
  - Long term, we could add a small Godot command‑line tool or editor script
    that:
    - Reads scene prompts directly from `docs/artifacts/scene_prompts/`.
    - Invokes NL Scene Builder logic programmatically.
  - For now, manual copy/paste into the dock is acceptable.

This doc is a target for Task Master implementation tasks; it does not
introduce any runtime changes by itself.

