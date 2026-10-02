#!/usr/bin/env python3
"""
CHECK_FATAL(fail_cond, fmt, ...)  ->  WC_ASSERT(invariant, fmt, ...)

CHECK_FATAL fires when its condition is TRUE (the failure condition).
WC_ASSERT fires when its condition is FALSE (the invariant). Every condition
is therefore negated. The script produces the most readable negation it can
prove correct:

  relational      i >= n            ->  i < n
  logical not     !p                ->  p
  De Morgan       a >= n || b >= m  ->  a < n && b < m
  fallback        anything else     ->  !(original)

Every non-fallback rewrite is verified: the new condition and !(original)
are evaluated over random integer assignments of every atom and must agree.
A rewrite that cannot be verified falls back to !(original), which is
correct by construction.

Usage:
  tools/convert_check_fatal.py [--dry-run] [--report FILE] ROOT
Rewrites src/, include/ and tests/ under ROOT in place.
"""

import argparse
import itertools
import os
import random
import re
import sys

CALL = "CHECK_FATAL("
NEW = "WC_ASSERT("

NEGATE = {"==": "!=", "!=": "==", "<": ">=", ">=": "<", ">": "<=", "<=": ">"}
CMP_OPS = ("==", "!=", "<=", ">=", "<", ">")


# ---------------------------------------------------------------- lexing helpers

def split_top_level(text, sep=","):
    """Split on `sep` at paren/bracket/brace depth 0, outside string/char literals."""
    parts, depth, cur, i, quote = [], 0, [], 0, None
    while i < len(text):
        c = text[i]
        if quote:
            cur.append(c)
            if c == "\\":
                cur.append(text[i + 1])
                i += 2
                continue
            if c == quote:
                quote = None
        elif c in "\"'":
            quote = c
            cur.append(c)
        elif c in "([{":
            depth += 1
            cur.append(c)
        elif c in ")]}":
            depth -= 1
            cur.append(c)
        elif depth == 0 and text.startswith(sep, i):
            parts.append("".join(cur))
            cur = []
            i += len(sep)
            continue
        else:
            cur.append(c)
        i += 1
    parts.append("".join(cur))
    return parts


def find_call_end(text, open_idx):
    """Index of the ')' matching the '(' at open_idx."""
    depth, i, quote = 0, open_idx, None
    while i < len(text):
        c = text[i]
        if quote:
            if c == "\\":
                i += 2
                continue
            if c == quote:
                quote = None
        elif c in "\"'":
            quote = c
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise ValueError("unbalanced call")


def top_level_ops(expr):
    """Operators appearing at depth 0, as (index, op). Handles ->, <<, >>, <=, >=."""
    ops, depth, i, quote = [], 0, 0, None
    while i < len(expr):
        c = expr[i]
        two = expr[i : i + 2]
        if quote:
            if c == "\\":
                i += 2
                continue
            if c == quote:
                quote = None
            i += 1
            continue
        if c in "\"'":
            quote = c
        elif c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif depth == 0:
            if two in ("->", "<<", ">>", "++", "--"):
                i += 2
                continue
            if two in ("&&", "||", "==", "!=", "<=", ">="):
                ops.append((i, two))
                i += 2
                continue
            if c in "<>?|&^=":
                ops.append((i, c))
        i += 1
    return ops


def strip_outer_parens(expr):
    e = expr.strip()
    while e.startswith("(") and e.endswith(")"):
        try:
            if find_call_end(e, 0) == len(e) - 1:
                e = e[1:-1].strip()
                continue
        except ValueError:
            pass
        break
    return e


def is_unary_not_operand(expr):
    """`!x` where `!` applies to the whole expression."""
    e = expr.strip()
    if not e.startswith("!") or e.startswith("!="):
        return False
    rest = e[1:].strip()
    body = rest.lstrip("*&").strip()  # unary deref/address-of bind tighter than !
    # the operand must contain no top-level binary operator (else ! binds tighter)
    return not top_level_ops(body) and not re.search(r"[+\-*/%](?![>=])", _depth0(body).replace("->", "  "))


def _depth0(expr):
    """Characters of expr at depth 0 only (nested groups blanked)."""
    out, depth = [], 0
    for c in expr:
        if c in "([{":
            depth += 1
            out.append(" ")
        elif c in ")]}":
            depth -= 1
            out.append(" ")
        else:
            out.append(c if depth == 0 else " ")
    return "".join(out)


# ---------------------------------------------------------------- negation

def negate_simple(expr):
    """Negate expr if it is a single comparison or a whole-operand `!`. Else None."""
    e = strip_outer_parens(expr)
    if is_unary_not_operand(e):
        return strip_outer_parens(e[1:])
    ops = top_level_ops(e)
    if len(ops) == 1 and ops[0][1] in CMP_OPS:
        i, op = ops[0]
        return f"{e[:i].strip()} {NEGATE[op]} {e[i + len(op):].strip()}"
    return None


def negate(expr):
    """Return (new_condition, method)."""
    e = strip_outer_parens(" ".join(expr.split()))
    if e in ("1", "true"):
        return "0", "constant"
    simple = negate_simple(e)
    if simple is not None:
        return simple, "simple"
    ops = {op for _, op in top_level_ops(e)}
    for joiner, other in (("||", "&&"), ("&&", "||")):
        if ops and ops <= set(CMP_OPS) | {joiner} and joiner in ops:
            parts = split_top_level(e, joiner)
            negs = [negate_simple(p) for p in parts]
            if all(n is not None for n in negs):
                return f" {other} ".join(negs), "de-morgan"
    if not top_level_ops(e) and not re.search(r"[+\-*/%](?![>=])", _depth0(e).replace("->", "  ")):
        return f"!{e}", "fallback"  # primary expression (e.g. a call): no parens needed
    return f"!({e})", "fallback"


# ---------------------------------------------------------------- verification

ATOM_RE = re.compile(
    r"""
    (?:sizeof\s*\([^()]*\))                                   # sizeof(T)
  | (?:[*&]\s*)?                                              # optional deref/addr
    [A-Za-z_]\w*
    (?:\s*(?:->|\.)\s*[A-Za-z_]\w*                            # member chains
      |\s*\[[^\[\]]*\]                                        # subscripts
      |\s*\((?:[^()]|\([^()]*\))*\)                           # calls (1 nesting)
    )*
  | \b\d+[uUlL]*\b                                            # integer literals
    """,
    re.VERBOSE,
)


def to_python(expr, atoms):
    def sub(m):
        text = " ".join(m.group(0).split())
        if re.fullmatch(r"\d+[uUlL]*", text):
            return str(int(re.sub(r"[uUlL]", "", text)))
        atoms.setdefault(text, f"_a{len(atoms)}")
        return atoms[text]

    # Logical operators first, as placeholders: `&&` must not be read as two
    # address-of operators by ATOM_RE, and `and/or/not` must not become atoms.
    e = expr.replace("&&", " \x01 ").replace("||", " \x02 ")
    e = re.sub(r"!(?!=)", " \x03 ", e)
    e = re.sub(r"\((\w+)\)\s*(->|\.)", r"\1\2", e)  # (a)->x  ->  a->x
    py = ATOM_RE.sub(sub, e)
    return py.replace("\x01", "and").replace("\x02", "or").replace("\x03", "not")


def verify(original, new):
    """True if `new` == !(original) for random integer atom values."""
    atoms = {}
    try:
        p_old = to_python(original, atoms)
        p_new = to_python(new, atoms)
        names = list(atoms.values())
        if not names:
            return bool(eval(f"(not ({p_old})) == bool({p_new})"))
        rng = random.Random(0)
        values = [-2, -1, 0, 1, 2, 3, 7]
        cases = (
            itertools.product(values, repeat=len(names))
            if len(names) <= 4
            else ([rng.choice(values) for _ in names] for _ in range(4000))
        )
        for combo in cases:
            env = dict(zip(names, combo))
            if bool(eval(f"not ({p_old})", {}, env)) != bool(eval(p_new, {}, env)):
                return False
        return True
    except Exception:
        return False


# ---------------------------------------------------------------- rewrite

def rewrite_text(text, path, report):
    out, i = [], 0
    while True:
        j = text.find(CALL, i)
        if j < 0:
            out.append(text[i:])
            break
        # skip identifiers that merely end in CHECK_FATAL, and #define CHECK_FATAL
        prev = text[j - 1] if j else " "
        line_start = text.rfind("\n", 0, j) + 1
        # skip `#define CHECK_FATAL(...)` itself, not macros whose BODY uses it
        if re.match(r"\w", prev) or re.fullmatch(r"\s*#\s*define\s+", text[line_start:j]):
            out.append(text[i : j + len(CALL)])
            i = j + len(CALL)
            continue
        open_idx = j + len(CALL) - 1
        close_idx = find_call_end(text, open_idx)
        inner = text[open_idx + 1 : close_idx]
        args = split_top_level(inner, ",")
        cond_raw = args[0]
        cond_clean = " ".join(cond_raw.replace("\\\n", " ").split())
        new_cond, method = negate(cond_clean)
        if method in ("simple", "de-morgan") and not verify(cond_clean, new_cond):
            new_cond, method = f"!({strip_outer_parens(cond_clean)})", "fallback (unverified)"
        lead = cond_raw[: len(cond_raw) - len(cond_raw.lstrip())]
        new_inner = lead + new_cond + inner[len(cond_raw):]
        line_no = text.count("\n", 0, j) + 1
        report.append((path, line_no, method, cond_clean, new_cond))
        out.append(text[i:j])
        out.append(NEW + new_inner + ")")
        i = close_idx + 1
    return "".join(out)


def realign_backslashes(old_text, new_text):
    """Keep macro continuation backslashes in the column they had before."""
    col = None
    lines = new_text.split("\n")
    for k, line in enumerate(lines):
        if line.rstrip().endswith("\\") and NEW in line:
            body = line.rstrip()[:-1].rstrip()
            # column of the nearest neighbouring continuation line
            for nb in (k - 1, k + 1):
                if 0 <= nb < len(lines) and lines[nb].rstrip().endswith("\\") and NEW not in lines[nb]:
                    col = len(lines[nb].rstrip()) - 1
                    break
            if col is not None and len(body) < col:
                lines[k] = body.ljust(col) + "\\"
            else:
                lines[k] = body + " \\"
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--report")
    a = ap.parse_args()

    report = []
    for sub in ("src", "include", "tests"):
        for dirpath, _, files in os.walk(os.path.join(a.root, sub)):
            for f in sorted(files):
                if not f.endswith((".c", ".h")):
                    continue
                p = os.path.join(dirpath, f)
                old = open(p).read()
                if CALL not in old:
                    continue
                new = realign_backslashes(old, rewrite_text(old, os.path.relpath(p, a.root), report))
                if new != old and not a.dry_run:
                    open(p, "w").write(new)

    by_method = {}
    for r in report:
        by_method[r[2]] = by_method.get(r[2], 0) + 1
    lines = [f"{len(report)} CHECK_FATAL call sites converted: " +
             ", ".join(f"{k} {v}" for k, v in sorted(by_method.items())), ""]
    lines += [f"{p}:{n}  [{m}]\n    CHECK_FATAL({c})\n    WC_ASSERT({w})" for p, n, m, c, w in report]
    text = "\n".join(lines) + "\n"
    if a.report:
        open(a.report, "w").write(text)
    print(lines[0])
    if any("unverified" in r[2] for r in report):
        print("note: some rewrites could not be verified and used the !(...) fallback", file=sys.stderr)


if __name__ == "__main__":
    main()
