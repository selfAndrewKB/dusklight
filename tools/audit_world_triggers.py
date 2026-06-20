#!/usr/bin/env python3
"""Rank actor functions that combine player reads with world/event transitions."""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path


FUNCTION_RE = re.compile(
    r"(?m)^[ \t]*(?!if\b|for\b|while\b|switch\b|catch\b)"
    r"(?:[A-Za-z_][\w:<>,~*&\[\] \t]*[ \t]+)"
    r"(?P<name>[A-Za-z_~]\w*(?:::\w+)*)[ \t]*"
    r"\([^;{}]*\)[ \t]*(?:const[ \t]*)?\{"
)

SOURCE_PATTERNS = {
    "direct": re.compile(
        r"daPy_getPlayerActorClass\s*\(|dComIfGp_getPlayer\s*\(\s*0\s*\)|"
        r"dComIfGp_getLinkPlayer\s*\(|daPy_getLinkPlayerActorClass\s*\(|"
        r"fopAcM_searchPlayer(?:Distance|DistanceXZ|AngleY)\s*\("
    ),
    "cached": re.compile(r"\bmPlayer(?:Dist|Angle)\b"),
    "participation": re.compile(r"world_trigger::(?:evaluate|stateForSource)\s*\("),
}

SINK_PATTERNS = {
    "event": re.compile(
        r"fopAcM_order\w*Event\w*\s*\(|checkCommandDemoAccrpt\s*\(|"
        r"eventInfo\.onCondition\s*\("
    ),
    "switch": re.compile(r"(?:dComIfGs|fopAcM)_(?:on|off)\w*Switch\s*\("),
    "demo": re.compile(
        r"\bmDemoMode\s*(?:=|\+\+|--)|setActionMode\s*\([^;\n]*(?:DEMO|OPENING)|"
        r"setProcess\s*\([^;\n]*(?:demo|Demo)|"
        r"mCamera\.(?:Stop|Reset|Start)\s*\("
    ),
}


@dataclass
class Finding:
    path: Path
    line: int
    name: str
    score: int
    sources: list[str]
    sinks: list[str]
    world_trigger: bool
    coop_touched: bool

    @property
    def anchor(self) -> str:
        return f"{self.path.as_posix()}:{self.name}"


def sanitize(text: str) -> str:
    """Blank comments and literals while preserving offsets and newlines."""
    pattern = re.compile(
        r"//[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'",
        re.DOTALL,
    )

    def blank(match: re.Match[str]) -> str:
        return "".join("\n" if char == "\n" else " " for char in match.group(0))

    return pattern.sub(blank, text)


def closing_brace(text: str, opening: int) -> int | None:
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return index + 1
    return None


def finding_score(sources: list[str], sinks: list[str]) -> int:
    source_weight = 3 if "direct" in sources else 2 if "participation" in sources else 1
    sink_weight = 2 if "event" in sinks or "switch" in sinks else 1
    return source_weight + sink_weight


def scan_file(path: Path, repo: Path) -> list[Finding]:
    raw = path.read_text(encoding="utf-8", errors="replace")
    clean = sanitize(raw)
    findings: list[Finding] = []
    matches = list(FUNCTION_RE.finditer(clean))
    for index, match in enumerate(matches):
        opening = clean.find("{", match.start(), match.end())
        end = closing_brace(clean, opening)
        next_start = matches[index + 1].start() if index + 1 < len(matches) else len(clean)
        # Preprocessor alternatives can contain mutually exclusive braces. Keep the audit
        # function-scoped by capping an unmatched/overlong body at the next definition.
        if end is None or end > next_start:
            end = next_start
        body = clean[match.start():end]
        raw_body = raw[match.start():end]
        sources = [name for name, pattern in SOURCE_PATTERNS.items() if pattern.search(body)]
        sinks = [name for name, pattern in SINK_PATTERNS.items() if pattern.search(body)]
        if not sources or not sinks:
            continue
        findings.append(
            Finding(
                path=path.relative_to(repo),
                line=raw.count("\n", 0, match.start()) + 1,
                name=match.group("name"),
                score=finding_score(sources, sinks),
                sources=sources,
                sinks=sinks,
                world_trigger="world_trigger::" in raw_body,
                coop_touched="Co-op:" in raw_body,
            )
        )
    return findings


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default="src/d/actor", help="Repo-relative tree to scan")
    parser.add_argument("--min-score", type=int, default=3, help="Lowest score to print")
    parser.add_argument(
        "--only-converted", action="store_true", help="Print only functions using world_trigger"
    )
    parser.add_argument(
        "--require-anchor",
        action="append",
        default=[],
        help="Fail unless this substring appears in a path:function anchor",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo = Path(__file__).resolve().parents[1]
    root = repo / args.root
    if not root.is_dir():
        print(f"error: scan root does not exist: {root}", file=sys.stderr)
        return 2

    findings: list[Finding] = []
    for path in sorted(root.rglob("*")):
        if path.suffix in {".cpp", ".inc"}:
            findings.extend(scan_file(path, repo))
    findings.sort(key=lambda item: (-item.score, item.path.as_posix(), item.line))

    print("| Score | Function | Player source | Transition sink | Converted |")
    print("| ---: | --- | --- | --- | --- |")
    for finding in findings:
        if finding.score < args.min_score or (args.only_converted and not finding.world_trigger):
            continue
        converted = "world_trigger" if finding.world_trigger else "co-op touched" if finding.coop_touched else "no"
        print(
            f"| {finding.score} | `{finding.anchor}:{finding.line}` | "
            f"{', '.join(finding.sources)} | {', '.join(finding.sinks)} | {converted} |"
        )

    missing = [
        anchor
        for anchor in args.require_anchor
        if not any(anchor.lower() in finding.anchor.lower() for finding in findings)
    ]
    if missing:
        for anchor in missing:
            print(f"error: required anchor not found: {anchor}", file=sys.stderr)
        return 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
