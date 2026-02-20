#!/usr/bin/env python3

"""
make_single_header.py
=====================

Converts C header/source pairs into self-contained single-header libraries.

Layout expected:
    ./include/<name>.h
    ./src/<name>.c
Output:
    ./single_header/<name>_single.h

Usage:
    python make_single_header.py arena gen_vector string hashmap hashset
    python make_single_header.py --all
    python make_single_header.py arena --include-dir path/to/include --src-dir path/to/src

Each output file:
  - Inlines all its declared dependencies (other components from this lib)
  - Wraps its implementation in  #ifdef WC_IMPLEMENTATION / #ifndef WC_<NAME>_IMPL
    so that the impl block emits exactly once per translation unit even when
    multiple single-headers are included together
"""

import argparse
import re
import sys
from pathlib import Path


# Known components and their dependency order.
# A component's dependencies must appear BEFORE it in this list.
# =========================================================================
COMPONENTS = [
    "common",
    "wc_errno",
    "fast_math",
    "arena",
    "gen_vector",
    "bit_vector",
    "String",
    "Stack",
    "Queue",
    "map_setup",
    "random",
    "hashmap",
    "hashset",
    "matrix",
    "matrix_generic",
    "helpers",
]

# Add a set of components that are dependency-only (no standalone output)
INTERNAL_COMPONENTS = [
    "common", 
    "wc_errno", 
    "map_setup",
]


# TODO: i dont want map_setup, common, wc_errno to have their own independent files

# Maps a component name to the names it depends on (other lib components only).
# Only direct dependencies are listed; transitive ones are resolved automatically.
DEPENDENCIES: dict[str, list[str]] = {
    "common":           [],
    "wc_errno":         [],
    "fast_math":        ["common"],
    "arena":            ["common", "wc_errno"],
    "gen_vector":       ["common", "wc_errno"],
    "bit_vector":       ["gen_vector"],
    "String":           ["gen_vector"],
    "Stack":            ["gen_vector"],
    "Queue":            ["gen_vector"],
    "map_setup":        ["String"],
    "random":           ["fast_math"],
    "hashmap":          ["map_setup"],
    "hashset":          ["map_setup"],
    "matrix":           ["arena"],
    "matrix_generic":   ["arena"],
    "helpers":          ["String"],
}


# Helpers
# =========================================================================

def guard_name(component: str) -> str:
    """WC_GEN_VECTOR_H  /  WC_STRING_H  etc."""
    return f"WC_{component.upper().replace('-', '_')}_H"


def impl_guard_name(component: str) -> str:
    return f"WC_{component.upper().replace('-', '_')}_IMPL"


def read_file(path: Path) -> str:
    if not path.exists():
        return ""
    return path.read_text(encoding="utf-8")


def strip_include_guards(src: str, component: str) -> str:
    """Remove the outermost #ifndef / #define / #endif include-guard pair."""
    # Match  #ifndef FOO_H  /  #define FOO_H  at the very start (ignoring blank lines)
    src = re.sub(
        r"^\s*#ifndef\s+\w+_H\s*\n\s*#define\s+\w+_H\s*\n",
        "",
        src,
        count=1,
        flags=re.MULTILINE,
    )
    # Remove the matching closing  #endif // ...  at the very end
    src = re.sub(
        r"\n\s*#endif\s*(?://[^\n]*)?\s*$",
        "",
        src.rstrip(),
    )
    return src.strip()


def strip_pragma_once(src: str) -> str:
    return re.sub(r"^\s*#pragma\s+once\s*\n", "", src, flags=re.MULTILINE)


def remove_local_includes(src: str, known_headers: set[str]) -> str:
    """
    Remove  #include "foo.h"  lines whose basename is a known lib header.
    Third-party / stdlib includes (<stdio.h> etc.) are kept.
    """
    def replacer(m):
        header = m.group(1)
        basename = Path(header).stem          # "gen_vector" from "gen_vector.h"
        basename_lower = basename.lower()
        # Drop if it matches any known component (case-insensitive)
        for known in known_headers:
            if known.lower() == basename_lower:
                return ""
        return m.group(0)

    return re.sub(r'#include\s+"([^"]+)"\s*\n', replacer, src)


def resolve_deps(components: list[str]) -> list[str]:
    """
    Given a list of requested components, return a full ordered list that
    includes all transitive dependencies, with each component appearing only
    once in dependency order.
    """
    visited: list[str] = []

    def visit(name: str):
        if name in visited:
            return
        if name not in DEPENDENCIES:
            print(f"  [warn] unknown component '{name}', skipping", file=sys.stderr)
            return
        for dep in DEPENDENCIES[name]:
            visit(dep)
        visited.append(name)

    for c in components:
        visit(c)
    return visited


# ---------------------------------------------------------------------------
# Core builder
# ---------------------------------------------------------------------------

def build_single_header(
    component: str,
    include_dir: Path,
    src_dir: Path,
    all_component_names: set[str],
) -> str:
    """
    Build the complete single-header text for one component.
    All dependencies are inlined before this component's own content.
    """
    # Full dependency chain for this component (includes itself at the end)
    dep_chain = resolve_deps([component])

    guard = f"WC_{component.upper()}_SINGLE_H"   # WC_GEN_VECTOR_SINGLE_H


    lines: list[str] = []
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append("")
    lines.append(
        "/*"
        f"\n * {component}_single.h"
        "\n * Auto-generated single-header library."
        "\n *"
        "\n * In EXACTLY ONE .c file, before including this header:"
        "\n *     #define WC_IMPLEMENTATION"
        "\n *     #include \"" + component + '_single.h"'
        "\n *"
        "\n * All other files just:"
        "\n *     #include \"" + component + '_single.h"'
        "\n */"
    )
    lines.append("")

    # ------------------------------------------------------------------ #
    # 1. Inline each dependency's header declarations
    # ------------------------------------------------------------------ #
    for dep in dep_chain:
        h_path = include_dir / f"{dep}.h"
        h_src = read_file(h_path)
        if not h_src:
            print(f"  [warn] {dep}.h not found in {include_dir}", file=sys.stderr)
            continue

        dep_guard = guard_name(dep)
        h_src = strip_pragma_once(h_src)
        h_src = strip_include_guards(h_src, dep)
        h_src = remove_local_includes(h_src, all_component_names)
        h_src = h_src.strip()

        lines.append(f"/* ===== {dep}.h ===== */")
        lines.append(f"#ifndef {dep_guard}")
        lines.append(f"#define {dep_guard}")
        lines.append("")
        lines.append(h_src)
        lines.append("")
        lines.append(f"#endif /* {dep_guard} */")
        lines.append("")

    # ------------------------------------------------------------------ #
    # 2. Wrap all implementations in the WC_IMPLEMENTATION block
    # ------------------------------------------------------------------ #
    lines.append("#ifdef WC_IMPLEMENTATION")
    lines.append("")

    for dep in dep_chain:
        c_path = src_dir / f"{dep}.c"
        c_src = read_file(c_path)
        if not c_src:
            # Header-only component (e.g. map_setup, matrix_generic) — skip silently
            continue

        impl_guard = impl_guard_name(dep)
        c_src = remove_local_includes(c_src, all_component_names)
        c_src = c_src.strip()

        lines.append(f"/* ===== {dep}.c ===== */")
        lines.append(f"#ifndef {impl_guard}")
        lines.append(f"#define {impl_guard}")
        lines.append("")
        lines.append(c_src)
        lines.append("")
        lines.append(f"#endif /* {impl_guard} */")
        lines.append("")

    lines.append("#endif /* WC_IMPLEMENTATION */")
    lines.append("")
    lines.append(f"#endif /* {guard} */")
    lines.append("")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Generate single-header versions of WCtoolkit components.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Convert specific components:
  python make_single_header.py arena gen_vector string

  # Convert everything:
  python make_single_header.py --all

  # Custom directory layout:
  python make_single_header.py --all --include-dir inc --src-dir source

Available components:
  """ + "  ".join(COMPONENTS),
    )

    parser.add_argument(
        "components",
        nargs="*",
        metavar="COMPONENT",
        help="Component names to convert (e.g. arena gen_vector string)",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Convert all known components",
    )
    parser.add_argument(
        "--include-dir",
        default="../include",
        metavar="DIR",
        help="Directory containing .h files (default: ../include)",
    )
    parser.add_argument(
        "--src-dir",
        default="../src",
        metavar="DIR",
        help="Directory containing .c files (default: ../src)",
    )
    parser.add_argument(
        "--out-dir",
        default="../single_header",
        metavar="DIR",
        help="Output directory (default: ../single_header)",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="List all known components and exit",
    )

    args = parser.parse_args()

    if args.list:
        print("Known components (in dependency order):")
        for name in COMPONENTS:
            deps = DEPENDENCIES.get(name, [])
            dep_str = ", ".join(deps) if deps else "(none)"
            print(f"  {name:<20} deps: {dep_str}")
        return

    if args.all:
        targets = list(COMPONENTS)
    elif args.components:
        targets = args.components
    else:
        parser.print_help()
        sys.exit(1)

    # Validate component names
    unknown = [c for c in targets if c not in DEPENDENCIES]
    if unknown:
        print(f"[error] unknown components: {', '.join(unknown)}", file=sys.stderr)
        print(f"        run with --list to see available components", file=sys.stderr)
        sys.exit(1)

    include_dir = Path(args.include_dir)
    src_dir     = Path(args.src_dir)
    out_dir     = Path(args.out_dir)

    if not include_dir.exists():
        print(f"[error] include dir not found: {include_dir}", file=sys.stderr)
        sys.exit(1)
    if not src_dir.exists():
        print(f"[error] src dir not found: {src_dir}", file=sys.stderr)
        sys.exit(1)

    out_dir.mkdir(parents=True, exist_ok=True)

    all_component_names = set(COMPONENTS)

    print(f"Output → {out_dir}/")
    print()

    for component in targets:
        out_path = out_dir / f"{component}_single.h"
        print(f"  building {component}_single.h ...", end=" ", flush=True)

        content = build_single_header(
            component=component,
            include_dir=include_dir,
            src_dir=src_dir,
            all_component_names=all_component_names,
        )

        out_path.write_text(content, encoding="utf-8")
        size_kb = out_path.stat().st_size / 1024
        print(f"done  ({size_kb:.1f} KB)")

    print()
    print(f"Done. {len(targets)} file(s) written to {out_dir}/")


if __name__ == "__main__":
    main()
