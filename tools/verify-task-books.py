"""Check the task-book delivery: coverage, ownership, dependencies and local links."""

from __future__ import annotations

import json
from pathlib import Path
import re
from urllib.parse import unquote


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    book_root = root / "docs/superpowers/plans/2026-10-27-task-books"
    manifest = json.loads((book_root / "manifest.json").read_text(encoding="utf-8"))
    prompt_root = Path(manifest["promptRoot"])
    require(Path(manifest["projectRoot"]).resolve() == root, "Project root mismatch")
    groups = manifest["groups"]
    require(len(groups) == 11, "Expected 11 task groups")
    ids = [group["id"] for group in groups]
    require(ids == [f"{i:02}" for i in range(1, 12)], "Group IDs are incomplete")
    expected_tasks = {
        f"{prefix}{i:02}"
        for prefix, count in (("A", 8), ("B", 7), ("C", 10))
        for i in range(1, count + 1)
    }
    tasks = [task for group in groups for task in group["tasks"]]
    require(len(tasks) == len(set(tasks)) == 25, "Duplicate or missing tasks")
    require(set(tasks) == expected_tasks, "Task IDs do not match the original 25")
    require(all(2 <= len(group["tasks"]) <= 3 for group in groups), "Group size must be 2 or 3")
    modules = {module["id"]: module["path"] for module in manifest["modules"]}
    require(set(modules) == {f"M{i:02}" for i in range(1, 38)}, "Expected 37 CPP modules")
    owners = [module for group in groups for module in group["modules"]]
    require(len(owners) == len(set(owners)) == 37, "CPP module ownership is not unique")
    require(set(owners) == set(modules), "CPP ownership coverage mismatch")
    completed: set[str] = set()
    markdown_files = list(book_root.glob("*.md")) + [prompt_root / "00-使用说明.md"]
    for group in groups:
        require(set(group["requires"]) <= completed, f"Bad or cyclic dependency: {group['id']}")
        completed.add(group["id"])
        book_path = book_root / group["book"]
        prompt_path = prompt_root / group["prompt"]
        require(book_path.is_file(), f"Missing task book: {book_path}")
        require(prompt_path.is_file(), f"Missing prompt: {prompt_path}")
        book = book_path.read_text(encoding="utf-8")
        prompt = prompt_path.read_text(encoding="utf-8")
        for task in group["tasks"]:
            require(task in book and task in prompt, f"Task absent from delivery: {task}")
        for module in group["modules"]:
            require(modules[module] in book and modules[module] in prompt, f"CPP absent: {module}")
        for token in ("实施顺序", "通过标准", "seed", "日志", group["report"]):
            require(token in book, f"Missing task-book item {token}: {group['id']}")
        for token in (book_path.name, "00-execution-guide.md", "00-native-testing.md", group["report"]):
            require(token in prompt, f"Missing prompt reference {token}: {group['id']}")
        require("```" not in prompt, f"Prompt is not flat text: {prompt_path}")
        require(not re.search(r"(?mi)^(ROLE|CONTEXT|INSTRUCTIONS|STEPS|NARROWING):", prompt),
                f"Framework scaffolding in prompt: {prompt_path}")
        require(not re.search(r"TODO|TBD|\[you fill", prompt), f"Unfilled prompt: {prompt_path}")
    checked_links = 0
    for path in markdown_files:
        text = path.read_text(encoding="utf-8")
        require(text.count("```") % 2 == 0, f"Unclosed code fence: {path}")
        for match in re.finditer(r"\[[^\]\n]+\]\((<[^>]+>|[^)\n]+)\)", text):
            target = match.group(1).strip("<>")
            if target.startswith(("https://", "http://", "#", "mailto:")):
                continue
            target = unquote(target.split("#", 1)[0])
            linked = Path(target)
            if not linked.is_absolute():
                linked = path.parent / linked
            require(linked.exists(), f"Broken local link in {path.name}: {target}")
            checked_links += 1
    print(f"PASS: {len(groups)} groups; 25 tasks exactly once; 37 CPP owners unique")
    print(f"PASS: 2-3 tasks per group; dependencies follow the listed order")
    print(f"PASS: 11 books and 11 flat-text prompts; {checked_links} local links valid")


if __name__ == "__main__":
    main()
