"""Insert #include and NCC_RECORD_CALL() into C/C++ sources."""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path

from tree_sitter import Language, Node, Parser

INCLUDE_LINE = '#include "native_call_counter.h"\n'
RECORD_LINE = "NCC_RECORD_CALL();"
DEFAULT_EXCLUDE = frozenset({"ncc_record_call", "ncc_init", "ncc_dump"})


@dataclass
class Result:
    path: Path
    include_added: bool
    functions_instrumented: int
    content: str


def _language(path: Path) -> Language:
    if path.suffix.lower() in {".cpp", ".cc", ".cxx", ".hpp"}:
        import tree_sitter_cpp as mod

        return Language(mod.language())
    import tree_sitter_c as mod

    return Language(mod.language())


def _function_name(node: Node) -> str | None:
    for child in node.children:
        if child.type == "compound_statement":
            continue
        if child.type in {
            "function_declarator",
            "pointer_declarator",
            "reference_declarator",
            "array_declarator",
            "parenthesized_declarator",
            "declarator",
            "qualified_identifier",
            "identifier",
        }:
            stack = [child]
            while stack:
                current = stack.pop()
                if current.type in {"identifier", "field_identifier", "destructor_name"}:
                    text = current.text
                    return text.decode() if text else None
                stack.extend(current.children)
    return None


def _definitions(root: Node) -> list[tuple[str, Node]]:
    out: list[tuple[str, Node]] = []
    stack = [root]
    while stack:
        node = stack.pop()
        if node.type == "function_definition":
            body = next((c for c in node.children if c.type == "compound_statement"), None)
            name = _function_name(node)
            if body is not None and name:
                out.append((name, body))
        stack.extend(reversed([c for c in node.children if c.is_named]))
    out.sort(key=lambda item: item[1].start_byte)
    return out


def _has_record_call(content: str, body_start: int) -> bool:
    head = content[body_start : body_start + 160]
    return bool(re.search(r"\bNCC_RECORD_CALL\s*\(\s*\)", head.split(";", 1)[0]))


def _body_indent(content: str, body_start: int) -> str:
    i = body_start
    while i < len(content) and content[i] in " \t":
        i += 1
    if i < len(content) and content[i] == "\n":
        line = content[i + 1 : content.find("\n", i + 1)]
        match = re.match(r"^(\s*)", line)
        return match.group(1) if match else "  "
    return "  "


def _add_include(content: str) -> tuple[str, bool]:
    if "native_call_counter.h" in content:
        return content, False
    lines = content.splitlines(keepends=True)
    last_include = -1
    for index, line in enumerate(lines):
        if line.lstrip().startswith("#include"):
            last_include = index
    if last_include >= 0:
        offset = sum(len(lines[i]) for i in range(last_include + 1))
        return content[:offset] + INCLUDE_LINE + content[offset:], True
    return INCLUDE_LINE + content, True


def instrument(
    content: str,
    path: Path,
    *,
    exclude: frozenset[str] = DEFAULT_EXCLUDE,
) -> Result:
    content, include_added = _add_include(content)
    tree = Parser(_language(path)).parse(content.encode())

    insertions: list[tuple[int, str]] = []
    for name, body in _definitions(tree.root_node):
        if name in exclude:
            continue
        body_start = body.start_byte + 1
        if _has_record_call(content, body_start):
            continue
        indent = _body_indent(content, body_start)
        insertions.append((body_start, f"\n{indent}{RECORD_LINE}"))

    for offset, text in sorted(insertions, key=lambda item: item[0], reverse=True):
        content = content[:offset] + text + content[offset:]

    return Result(
        path=path,
        include_added=include_added,
        functions_instrumented=len(insertions),
        content=content,
    )


def instrument_file(
    path: Path,
    *,
    exclude: frozenset[str] = DEFAULT_EXCLUDE,
    dry_run: bool = False,
) -> Result:
    original = path.read_text(encoding="utf-8")
    result = instrument(original, path, exclude=exclude)
    if not dry_run and result.content != original:
        path.write_text(result.content, encoding="utf-8")
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Instrument C/C++ files with NCC_RECORD_CALL().")
    parser.add_argument("--file", action="append", required=True, type=Path, dest="files")
    parser.add_argument("--exclude", default=",".join(sorted(DEFAULT_EXCLUDE)))
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)
    exclude = frozenset(name.strip() for name in args.exclude.split(",") if name.strip())

    for path in args.files:
        result = instrument_file(path, exclude=exclude, dry_run=args.dry_run)
        mode = "would update" if args.dry_run else "updated"
        print(
            f"{path}: {mode}, include_added={result.include_added}, "
            f"functions_instrumented={result.functions_instrumented}"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
