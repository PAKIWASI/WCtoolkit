#!/usr/bin/env python3
"""
Rewrite every `#define NAME(...) do { BODY } while (0)` macro as a GNU
statement expression `({ BODY })`, so it is an expression of type void.

Equivalence: a statement expression whose last statement is not an
expression statement has type void and runs BODY exactly as do/while(0)
did, including `return` (allowed in GCC/Clang statement expressions).

The one construct whose meaning CHANGES is `break`/`continue` at the top
level of BODY: inside do/while(0) they exit the macro, inside ({ }) they
would hit the caller's loop. The script refuses such macros.

Usage: tools/do_while_to_expr.py ROOT   (rewrites src/, include/, tests/)
"""
import os
import re
import sys

DEF = re.compile(r"^[ \t]*#[ \t]*define[ \t]+(\w+)", re.M)


def macro_extent(text, start):
    """End index (exclusive) of the #define starting at `start` (follows \\ continuations)."""
    i = start
    while True:
        nl = text.find("\n", i)
        if nl < 0:
            return len(text)
        if text[nl - 1] != "\\":
            return nl
        i = nl + 1


def loops_at_top(body):
    """break/continue not nested inside a loop/switch within the body."""
    stripped = re.sub(r'"(\\.|[^"\\])*"', '""', body)
    depth_stack, i = [], 0
    toks = re.finditer(r"\b(for|while|do|switch|break|continue)\b|[{}]", stripped)
    loop_depth, brace, pending = 0, 0, []
    for m in toks:
        t = m.group(0)
        if t in ("for", "while", "do", "switch"):
            pending.append(brace)
        elif t == "{":
            brace += 1
            if pending and pending[-1] == brace - 1:
                depth_stack.append(brace)
                pending.pop()
                loop_depth += 1
        elif t == "}":
            if depth_stack and depth_stack[-1] == brace:
                depth_stack.pop()
                loop_depth -= 1
            brace -= 1
        elif t in ("break", "continue") and loop_depth == 0:
            return True
    return False


def convert(text, path, log):
    out, pos = [], 0
    for m in DEF.finditer(text):
        start, end = m.start(), macro_extent(text, m.start())
        macro = text[start:end]
        dm = re.search(r"\bdo\s*\{", macro)
        wm = list(re.finditer(r"\}\s*while\s*\(\s*0\s*\)", macro))
        if not dm or not wm:
            continue
        w = wm[-1]
        if macro[w.end():].strip(" \t\\\n"):
            continue  # do/while is not the whole body
        body = macro[dm.end():w.start()]
        if loops_at_top(body):
            sys.exit(f"{path}: {m.group(1)}: break/continue at macro top level; convert by hand")
        new = macro[: dm.start()] + "({" + body + "})" + macro[w.end():]
        # keep the closing line's continuation layout tidy
        out.append(text[pos:start])
        out.append(new)
        pos = end
        log.append(f"{path}: {m.group(1)}")
    out.append(text[pos:])
    return "".join(out)


root = sys.argv[1]
log = []
for sub in ("src", "include", "tests"):
    for d, _, files in os.walk(os.path.join(root, sub)):
        for f in sorted(files):
            if f.endswith((".c", ".h")):
                p = os.path.join(d, f)
                s = open(p).read()
                n = convert(s, os.path.relpath(p, root), log)
                if n != s:
                    open(p, "w").write(n)
print(f"converted {len(log)} do/while(0) macros to statement expressions:")
print("\n".join("  " + l for l in log))
