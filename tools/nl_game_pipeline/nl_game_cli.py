#!/usr/bin/env python3
"""
NL Game Orchestrator CLI (skeleton).

This tool wires together the design in docs/nl_game_orchestrator.md. For now it
only creates stub artifacts under docs/artifacts/ and prints where it wrote
them. No network calls or Godot automation are performed.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional
from urllib import error as urlerror
from urllib import request as urlrequest

REPO_ROOT = Path(__file__).resolve().parents[2]
ARTIFACT_ROOT = REPO_ROOT / "docs" / "artifacts"


@dataclass
class CliConfig:
    slug: str
    idea: str


def _ensure_dirs() -> None:
    (ARTIFACT_ROOT / "game_specs").mkdir(parents=True, exist_ok=True)
    (ARTIFACT_ROOT / "scene_prompts").mkdir(parents=True, exist_ok=True)
    (ARTIFACT_ROOT / "scene_expectations").mkdir(parents=True, exist_ok=True)
    (ARTIFACT_ROOT / "qa_reports").mkdir(parents=True, exist_ok=True)


def _normalise_slug(raw: str) -> str:
    slug = raw.strip().lower().replace(" ", "_")
    if not slug:
        slug = "game"
    return "".join(c for c in slug if (c.isalnum() or c in ("_", "-")))


def _load_openai_api_key() -> Optional[str]:
    """Load OPENAI_API_KEY from environment or .env file without logging it."""

    key = os.environ.get("OPENAI_API_KEY")
    if key:
        return key.strip()

    env_path = REPO_ROOT / ".env"
    if env_path.exists():
        for line in env_path.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("OPENAI_API_KEY="):
                return line.split("=", 1)[1].strip()
    return None


def _extract_required_elements(markdown: str) -> list[str]:
    items: list[str] = []
    for line in markdown.splitlines():
        stripped = line.strip()
        if stripped.startswith("- "):
            items.append(stripped[2:].strip())
    return items


def _scene_goal_from_spec(spec: Optional[dict], scene_id: str) -> str:
    if not spec:
        return ""
    for scene in spec.get("scenes", []):
        if scene.get("id") == scene_id:
            return scene.get("description", "")
    return ""


def _build_capsule_payload(
    cfg: CliConfig,
    scene_id: str,
    scene_goal: str,
    prompt_md: str,
    checklist_md: str,
    spec_data: Optional[dict],
) -> dict:
    spec_excerpt: dict[str, str] = {}
    if spec_data:
        for key in ("title", "core_loop", "genre"):
            value = spec_data.get(key)
            if isinstance(value, str):
                spec_excerpt[key] = value

    required_elements = _extract_required_elements(checklist_md)

    return {
        "capsule_version": 1,
        "slug": cfg.slug,
        "scene_id": scene_id,
        "idea": cfg.idea,
        "scene_goal": scene_goal or cfg.idea,
        "prompt_md": prompt_md,
        "checklist_md": checklist_md,
        "required_elements": required_elements,
        "resource_hints": [],
        "constraints": [],
        "llm_passes": ["layout"],
        "existing_context": {
            "spec_excerpt": spec_excerpt,
            "notes": spec_data.get("notes", []) if isinstance(spec_data, dict) else [],
            "scene_summary": "",
        },
    }


def _write_capsule_file(
    cfg: CliConfig,
    prompts_dir: Path,
    scene_id: str,
    scene_goal: str,
    prompt_md: str,
    checklist_md: str,
    spec_data: Optional[dict],
) -> Path:
    capsule = _build_capsule_payload(cfg, scene_id, scene_goal, prompt_md, checklist_md, spec_data)
    capsule_path = prompts_dir / f"{scene_id}.capsule.json"
    capsule_path.write_text(json.dumps(capsule, indent=2), encoding="utf-8")
    return capsule_path


def _openai_chat(system_prompt: str, user_prompt: str, model: str = "gpt-4.1-mini") -> Optional[str]:
    """Call OpenAI's chat/completions API and return the assistant content.

    If the API key is missing or a network/error occurs, returns None.
    """

    api_key = _load_openai_api_key()
    if not api_key:
        return None

    url = "https://api.openai.com/v1/chat/completions"
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {api_key}",
    }
    payload = {
        "model": model,
        "messages": [
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": user_prompt},
        ],
        "temperature": 0.2,
    }

    data = json.dumps(payload).encode("utf-8")
    req = urlrequest.Request(url, data=data, headers=headers, method="POST")

    try:
        with urlrequest.urlopen(req, timeout=30) as resp:
            body = resp.read().decode("utf-8")
    except (urlerror.URLError, TimeoutError, OSError):
        return None

    try:
        parsed = json.loads(body)
        return parsed["choices"][0]["message"]["content"]
    except Exception:
        return None


def _enrich_spec_with_llm(spec: dict, cfg: CliConfig) -> None:
    """Optionally enrich the stub spec's core_loop and notes using OpenAI."""

    system = (
        "You are a 2D Godot game design assistant. "
        "Given a short game idea, summarize the core gameplay loop "
        "and provide a few implementation notes. "
        "Keep everything feasible for a small 2D Godot 4 game."
    )
    user = (
        "Game idea:\n"
        f"{cfg.idea}\n\n"
        "Reply in the following plain-text format:\n"
        "CORE_LOOP: <one concise sentence>\n"
        "NOTES:\n"
        "- <short note 1>\n"
        "- <short note 2>\n"
        "- <short note 3>\n"
    )

    content = _openai_chat(system, user)
    if not content:
        return

    lines = [l.strip() for l in content.splitlines() if l.strip()]
    core_loop_line = next((l for l in lines if l.upper().startswith("CORE_LOOP:")), None)
    if core_loop_line:
        spec["core_loop"] = core_loop_line.split(":", 1)[1].strip() or spec.get("core_loop", "")

    notes_start = None
    for idx, line in enumerate(lines):
        if line.upper().startswith("NOTES"):
            notes_start = idx + 1
            break
    if notes_start is not None:
        for line in lines[notes_start:]:
            if line.startswith("-"):
                note = line.lstrip("-").strip()
                if note:
                    spec.setdefault("notes", []).append(note)


def _cmd_spec(cfg: CliConfig) -> Path:
    _ensure_dirs()
    spec_path = ARTIFACT_ROOT / "game_specs" / f"{cfg.slug}.json"

    # Start with a deterministic stub spec.
    spec = {
        "id": cfg.slug,
        "title": cfg.idea[:64],
        "genre": "unknown",
        "core_loop": cfg.idea,
        "camera": {"type": "top_down", "follows_player": True},
        "player": {
            "name": "Player",
            "description": "Auto-generated player stub.",
            "body_type": "CharacterBody2D",
            "controls": [
                "move with WASD or arrow keys",
            ],
            "stats": {"max_health": 3, "move_speed": 200},
        },
        "enemies": [],
        "collectibles": [],
        "ui": {"show_health": True, "show_score": True, "other_elements": []},
        "scenes": [
            {
                "id": "main_menu",
                "type": "menu",
                "description": "Simple start screen.",
                "entry_point": True,
            },
            {
                "id": "level_1",
                "type": "level",
                "description": "Single level stub based on the idea.",
                "entry_point": False,
            },
        ],
        "win_condition": "Not specified (stub).",
        "lose_condition": "Not specified (stub).",
        "notes": ["Generated by nl_game_cli.py from a single idea string."],
    }

    # If we can reach OpenAI, enrich the core loop and notes using the idea.
    _enrich_spec_with_llm(spec, cfg)

    spec_path.write_text(json.dumps(spec, indent=2), encoding="utf-8")
    return spec_path


def _cmd_prompts(cfg: CliConfig) -> list[Path]:
    _ensure_dirs()
    prompts_dir = ARTIFACT_ROOT / "scene_prompts" / cfg.slug
    expectations_dir = ARTIFACT_ROOT / "scene_expectations" / cfg.slug
    prompts_dir.mkdir(parents=True, exist_ok=True)
    expectations_dir.mkdir(parents=True, exist_ok=True)

    created: list[Path] = []

    # If we have a spec and API key, ask OpenAI to generate prompts/checklists.
    spec_path = ARTIFACT_ROOT / "game_specs" / f"{cfg.slug}.json"
    spec_data: Optional[dict] = None
    if spec_path.exists():
        try:
            spec_data = json.loads(spec_path.read_text(encoding="utf-8"))
        except Exception:
            spec_data = None

    if spec_data and _load_openai_api_key():
        scenes = spec_data.get("scenes") or []
        system = (
            "You are a Scene Builder Agent for small 2D Godot games. "
            "Given a structured 2D game spec, you design per-scene natural "
            "language prompts for an \"NL Scene Builder\" editor dock and "
            "concise Markdown checklists of expected nodes/behaviours.\n\n"
            "Respond with ONLY valid JSON using this schema:\n"
            "{\n"
            "  \"scenes\": [\n"
            "    {\n"
            "      \"id\": \"scene_id (string)\",\n"
            "      \"prompt_md\": \"Markdown text for the NL prompt\",\n"
            "      \"checklist_md\": \"Markdown checklist for that scene\"\n"
            "    }\n"
            "  ]\n"
            "}\n"
        )
        user = (
            "Game spec JSON:\n"
            f"{json.dumps(spec_data, indent=2)}\n\n"
            "For each scene in spec.scenes, produce one entry in the JSON "
            "array. Use the scene id field from the spec. Keep prompts and "
            "checklists short and feasible for a tiny 2D Godot game."
        )

        content = _openai_chat(system, user, model="gpt-4.1-mini")
        if content:
            try:
                payload = json.loads(content)
                for scene_entry in payload.get("scenes", []):
                    scene_id = scene_entry.get("id") or "unnamed_scene"
                    prompt_md = scene_entry.get("prompt_md") or ""
                    checklist_md = scene_entry.get("checklist_md") or ""
                    scene_goal = _scene_goal_from_spec(spec_data, scene_id)

                    prompt_path = prompts_dir / f"{scene_id}.prompt.md"
                    prompt_path.write_text(prompt_md, encoding="utf-8")
                    created.append(prompt_path)

                    checklist_path = expectations_dir / f"{scene_id}.checklist.md"
                    checklist_path.write_text(checklist_md, encoding="utf-8")
                    created.append(checklist_path)

                    capsule_path = _write_capsule_file(
                        cfg,
                        prompts_dir,
                        scene_id,
                        scene_goal,
                        prompt_md,
                        checklist_md,
                        spec_data,
                    )
                    created.append(capsule_path)

                # If we managed to parse JSON and write files, return now.
                if created:
                    return created
            except Exception:
                # Fall through to stub generation below.
                created.clear()

    main_prompt = (
        "# NL Scene Builder Prompt: main_menu\n\n"
        "Create a simple main menu for the game idea:\n"
        f"\"{cfg.idea}\"\n\n"
        "- Add a title label with the game name.\n"
        "- Add a \"Start\" button that will later start the first level.\n"
        "- Keep visuals simple.\n"
    )
    main_prompt_path = prompts_dir / "main_menu.prompt.md"
    main_prompt_path.write_text(main_prompt, encoding="utf-8")
    created.append(main_prompt_path)

    main_expect = (
        "Scene: main_menu\n\n"
        "Summary:\n"
        "- Main menu with title and Start button.\n\n"
        "Required nodes:\n"
        "- TitleLabel: Label\n"
        "- StartButton: Button\n\n"
        "Important properties:\n"
        "- Button text reads \"Start\".\n\n"
        "Signals/interactions:\n"
        "- StartButton pressed will later change to the first level.\n"
    )
    main_expect_path = expectations_dir / "main_menu.checklist.md"
    main_expect_path.write_text(main_expect, encoding="utf-8")
    created.append(main_expect_path)

    main_capsule_path = _write_capsule_file(
        cfg,
        prompts_dir,
        "main_menu",
        "Simple start screen with title and Start button.",
        main_prompt,
        main_expect,
        spec_data,
    )
    created.append(main_capsule_path)

    level_prompt = (
        "# NL Scene Builder Prompt: level_1\n\n"
        "Create a simple 2D level for the game idea:\n"
        f"\"{cfg.idea}\"\n\n"
        "- Place a Player character that can move.\n"
        "- Add a few placeholder obstacles or enemies.\n"
        "- Show health and score UI using simple nodes.\n"
    )
    level_prompt_path = prompts_dir / "level_1.prompt.md"
    level_prompt_path.write_text(level_prompt, encoding="utf-8")
    created.append(level_prompt_path)

    level_expect = (
        "Scene: level_1\n\n"
        "Summary:\n"
        "- Single level where the player interacts with basic obstacles or enemies.\n\n"
        "Required nodes:\n"
        "- Player: CharacterBody2D with movement script.\n"
        "- At least one obstacle or enemy node.\n"
        "- Health UI and Score UI.\n\n"
        "Important properties:\n"
        "- Player movement speed is reasonable.\n\n"
        "Signals/interactions:\n"
        "- Collisions or interactions will later adjust health/score.\n"
    )
    level_expect_path = expectations_dir / "level_1.checklist.md"
    level_expect_path.write_text(level_expect, encoding="utf-8")
    created.append(level_expect_path)

    level_capsule_path = _write_capsule_file(
        cfg,
        prompts_dir,
        "level_1",
        "Single level with player, obstacles, and HUD.",
        level_prompt,
        level_expect,
        spec_data,
    )
    created.append(level_capsule_path)

    return created


def _cmd_qa(cfg: CliConfig) -> Path:
    _ensure_dirs()
    report_path = ARTIFACT_ROOT / "qa_reports" / f"{cfg.slug}.md"

    # Try to load existing spec and expectations to feed into the QA agent.
    spec_path = ARTIFACT_ROOT / "game_specs" / f"{cfg.slug}.json"
    spec_text = ""
    if spec_path.exists():
        spec_text = spec_path.read_text(encoding="utf-8")

    expectations_dir = ARTIFACT_ROOT / "scene_expectations" / cfg.slug
    expectations_text = ""
    if expectations_dir.exists():
        parts: list[str] = []
        for p in sorted(expectations_dir.glob("*.checklist.md")):
            parts.append(f"# {p.name}\n")
            parts.append(p.read_text(encoding="utf-8"))
        expectations_text = "\n\n".join(parts)

    # If we have an API key, ask OpenAI to draft a QA report using our template.
    qa_markdown: Optional[str] = None
    if _load_openai_api_key():
        system = (
            "You are a QA Tester Agent for small 2D Godot games. "
            "Given a structured game spec and per-scene expectations, "
            "write a concise QA report in Markdown.\n\n"
            "Use this structure:\n"
            "# QA Report: <game_slug>\n\n"
            "## Summary\n"
            "- ...\n\n"
            "## Structural Checks by Scene\n"
            "### Scene: <scene_id>\n"
            "Expected:\n"
            "- ...\n"
            "Observed:\n"
            "- ...\n\n"
            "## Behavioural Assumptions\n"
            "- ...\n\n"
            "## UI and Feedback\n"
            "- ...\n\n"
            "## Recommended Next Steps\n"
            "- ...\n"
        )
        user = (
            f"Game slug: {cfg.slug}\n\n"
            "Game spec JSON (may be empty):\n"
            f"{spec_text}\n\n"
            "Scene expectation checklists (may be empty):\n"
            f"{expectations_text}\n\n"
            "Write the QA report following the structure above. "
            "If information is missing, make reasonable assumptions but keep "
            "the report honest about what was not evaluated."
        )
        qa_markdown = _openai_chat(system, user, model="gpt-4.1-mini")

    if not qa_markdown:
        # Fallback stub report if we couldn't call OpenAI.
        qa_markdown = (
            f"# QA Report: {cfg.slug}\n\n"
            "## Summary\n"
            "- Stub QA report generated by nl_game_cli.py.\n"
            "- This report does not reflect an actual build; it is a template.\n\n"
            "## Structural Checks by Scene\n\n"
            "### Scene: main_menu\n\n"
            "Expected:\n"
            "- Title label and Start button.\n\n"
            "Observed:\n"
            "- Not evaluated (stub).\n\n"
            "### Scene: level_1\n\n"
            "Expected:\n"
            "- Player, obstacles/enemies, health and score UI.\n\n"
            "Observed:\n"
            "- Not evaluated (stub).\n\n"
            "## Behavioural Assumptions\n\n"
            "- Player moves in a simple 2D space.\n"
            "- Basic interactions will later be added.\n\n"
            "## UI and Feedback\n\n"
            "- Health and score UI planned but not checked.\n\n"
            "## Recommended Next Steps\n\n"
            "- Implement the actual game.\n"
            "- Run real QA tools once scenes are generated.\n"
        )

    report_path.write_text(qa_markdown, encoding="utf-8")
    return report_path


def _write_run_log(cfg: CliConfig, spec_path: Path, prompt_paths: list[Path], qa_path: Path) -> None:
    """Record a simple JSON log entry for a `new` run."""

    runs_dir = ARTIFACT_ROOT / "runs"
    runs_dir.mkdir(parents=True, exist_ok=True)

    log = {
        "slug": cfg.slug,
        "idea": cfg.idea,
        "created_at": datetime.now(timezone.utc).isoformat(),
        "spec": str(spec_path.relative_to(REPO_ROOT)) if spec_path else None,
        "prompts": [str(p.relative_to(REPO_ROOT)) for p in prompt_paths],
        "qa_report": str(qa_path.relative_to(REPO_ROOT)) if qa_path else None,
    }

    log_path = runs_dir / f"{cfg.slug}.json"
    log_path.write_text(json.dumps(log, indent=2), encoding="utf-8")


def _cmd_runs(slug: Optional[str]) -> int:
    """List previous `new` runs, or show details for one slug."""

    runs_dir = ARTIFACT_ROOT / "runs"
    if not runs_dir.exists():
        print("No runs recorded yet.")
        return 0

    if slug:
        path = runs_dir / f"{_normalise_slug(slug)}.json"
        if not path.exists():
            print(f"No run log found for slug {slug!r}.")
            return 1
        data = json.loads(path.read_text(encoding="utf-8"))
        print(json.dumps(data, indent=2))
        return 0

    logs = sorted(runs_dir.glob("*.json"))
    if not logs:
        print("No runs recorded yet.")
        return 0

    for path in logs:
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except Exception:
            continue
        slug_val = data.get("slug") or path.stem
        created = data.get("created_at", "?")
        idea = (data.get("idea") or "").strip()
        if len(idea) > 60:
            idea = idea[:57] + "..."
        print(f"{slug_val:15} {created:25} {idea}")

    return 0


def _parse_args(argv: Optional[list[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="NL Game Orchestrator CLI (skeleton)")
    sub = parser.add_subparsers(dest="command", required=True)

    def add_common(p: argparse.ArgumentParser) -> None:
        p.add_argument("idea", help="Short natural language game idea.")
        p.add_argument("--slug", help="Slug for artifacts (defaults from idea).")

    p_spec = sub.add_parser("spec", help="Generate a stub game spec.")
    add_common(p_spec)

    p_prompts = sub.add_parser("prompts", help="Generate stub scene prompts and expectations.")
    add_common(p_prompts)

    p_qa = sub.add_parser("qa", help="Generate a stub QA report.")
    add_common(p_qa)

    p_new = sub.add_parser("new", help="Run spec, prompts, and qa in one go.")
    add_common(p_new)

    p_runs = sub.add_parser("runs", help="List or inspect previous runs.")
    p_runs.add_argument("--slug", help="Slug to inspect (default: list all).")

    return parser.parse_args(argv)


def main(argv: Optional[list[str]] = None) -> int:
    args = _parse_args(argv)
    if args.command == "runs":
        return _cmd_runs(getattr(args, "slug", None))

    slug = _normalise_slug(args.slug or args.idea)
    cfg = CliConfig(slug=slug, idea=args.idea)

    if args.command == "spec":
        path = _cmd_spec(cfg)
        print(f"Spec written to {path.relative_to(REPO_ROOT)}")
    elif args.command == "prompts":
        paths = _cmd_prompts(cfg)
        for p in paths:
            print(f"Created {p.relative_to(REPO_ROOT)}")
    elif args.command == "qa":
        path = _cmd_qa(cfg)
        print(f"QA report written to {path.relative_to(REPO_ROOT)}")
    elif args.command == "new":
        spec_path = _cmd_spec(cfg)
        prompt_paths = _cmd_prompts(cfg)
        qa_path = _cmd_qa(cfg)
        print(f"Spec written to {spec_path.relative_to(REPO_ROOT)}")
        for p in prompt_paths:
            print(f"Created {p.relative_to(REPO_ROOT)}")
        print(f"QA report written to {qa_path.relative_to(REPO_ROOT)}")
        _write_run_log(cfg, spec_path, prompt_paths, qa_path)
    else:
        raise SystemExit(f"Unknown command: {args.command}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
