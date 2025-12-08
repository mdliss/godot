# QA Tester Agent and Game Validation Strategy

The **QA Tester Agent** evaluates whether a generated 2D game matches its
intended design at a structural and basic behavioural level. It does not fully
replace playtesting, but it can catch obvious gaps early.

This document defines the QA Tester Agent’s role, the inputs it expects, and
the shape of its outputs (QA report and simple test plan).

---

## Role and Responsibilities

**Inputs:**

- Game spec (from Game Spec Agent).
- Per‑scene expectation checklists (from Scene Builder Agent).
- Optionally, a summary of the generated project, for example:
  - Scene tree dumps.
  - Script names/paths.
  - Basic connection info (signals, autoloads).

**Outputs:**

- A **QA report** for the game:
  - Overall summary.
  - Structural checks vs. expectations.
  - Basic behavioural assumptions and risks.
  - UI checks.
  - Recommendations / next steps.
- A short **test plan**:
  - A list of manual or automated checks that a human or future test runner
    should perform.

Artifacts (proposed):

- `docs/artifacts/qa_reports/<game_slug>.md`

---

## Reasonable Checks (MVP)

Given the current scope (2D Godot, NL Scene Builder, simple scripts), the QA
Tester Agent should focus on:

1. **Structural completeness**
   - For each scene, compare the expectation checklist to a scene summary:
     - Are the required node types present?
     - Are key UI elements present (health, score)?
     - Are there scripts attached where expected (e.g., player, enemies)?

2. **Basic behavioural assumptions**
   - Does the game spec describe movement, scores, win/lose conditions?
   - Does the scene summary show plausible hooks for:
     - Player movement (e.g., scripts with `_physics_process`).
     - Enemy spawning or behaviour.
     - Score/health changes.

3. **UI correctness**
   - Are required HUD elements present in the scenes that need them?
   - Do names/texts look coherent with the spec?

The agent does **not** need to prove correctness—it flags likely problems and
gaps.

---

## QA Report Template

The QA report is a Markdown document structured roughly as follows:

```md
# QA Report: <Game Title> (<game_slug>)

## Summary
- High-level description of the game and what was examined.
- Overall verdict: e.g., "Spec mostly satisfied with a few missing UI elements."

## Structural Checks by Scene

### Scene: <scene_id> (<scene_name>)

Expected (from checklist):
- <short bullet list of key expectations>

Observed (from scene summary):
- <short bullet list of what was found>

Findings:
- ✅ <good news>
- ⚠️ <minor issue>
- ❌ <missing critical element>

---

## Behavioural Assumptions

- Movement: <notes on whether a player controller script exists, uses input actions, etc.>
- Enemies: <notes on enemy scripts, timers, or spawners>
- Scoring / Progression: <notes on score updates, win/lose hooks>

---

## UI and Feedback

- Health display: <present/missing, correctness>
- Score display: <present/missing, correctness>
- Menus / transitions: <main menu / game over basics>

---

## Recommended Next Steps

- <short list of concrete changes to spec, prompts, or scenes>

## Test Plan Sketch

- Manual tests:
  - <list of simple in‑editor or in‑game checks to run>
- Optional automated tests:
  - <placeholder ideas for GDScript or CLI‑driven tests>
```

The agent may not literally use emojis or the exact symbols above, but the
structure (Summary → per‑scene checks → behaviour → UI → recommendations →
test plan) should be preserved.

---

## Inputs in Practice

The orchestrator is expected to provide the QA Tester Agent with:

- **Game spec JSON** (or equivalent), as produced by the Game Spec Agent.
- **Expectation checklists** for each scene, as Markdown or JSON.
- **Scene summaries**, which could be:
  - A JSON dump of node trees.
  - A list of scenes and top‑level node names/types.
  - A list of scripts and their attached nodes.

The QA Tester Agent does not talk directly to Godot; it reasons over these
summaries and documents.

---

## Agent Prompt Sketch

Conceptual prompt to drive the QA Tester Agent:

> **System**  
> You are a QA design assistant for small 2D Godot games.  
> You compare a structured game spec and per‑scene expectations against simple
> summaries of what was actually generated, and you write a concise QA report
> plus a short test plan. You focus on structural completeness and obvious
> behavioural gaps, not subtle gameplay balance.
>
> **User**  
> 1. Game spec (JSON):  
>    `<GAME_SPEC_JSON>`  
> 2. Scene expectations (Markdown or JSON) for each scene:  
>    `<CHECKLISTS>`  
> 3. Scene summaries (JSON) of the generated game:  
>    `<SCENE_SUMMARIES_JSON>`  
> 4. Tasks:  
>    - For each scene, compare expectations vs. summaries and list findings.  
>    - Highlight missing critical elements (e.g., no player, no enemies, no HUD).  
>    - Identify likely behavioural gaps from the summaries.  
>    - Produce a Markdown QA report using the given template.  
>    - Propose a small set of manual/automated checks in a “Test Plan Sketch”
>      section.

The orchestrator will be responsible for formatting `CHECKLISTS` and
`SCENE_SUMMARIES_JSON` in a way that fits available context limits.

