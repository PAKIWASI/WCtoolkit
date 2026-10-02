#!/usr/bin/env python3
"""
Rename the test harness's non-fatal checks from WC_ASSERT* to WC_EXPECT*.

The library now owns WC_ASSERT (debug-only, aborting invariant check). The
harness macros never aborted: a failure is counted and the test continues,
which is EXPECT semantics. Only names DEFINED by the harness headers in
tests/ are renamed; library macros that share the prefix (WC_ASSERT_ELEM_SIZE
in wc_macros.h) are left alone.

Usage: tools/rename_test_asserts.py ROOT
"""
import os
import re
import sys

root = sys.argv[1]
tests = os.path.join(root, "tests")

# names the harness defines
harness = set()
for f in os.listdir(tests):
    if f.endswith(".h"):
        harness |= set(re.findall(r"#define\s+(WC_ASSERT\w*)", open(os.path.join(tests, f)).read()))
library = set()
for d, _, files in os.walk(os.path.join(root, "include")):
    for f in files:
        library |= set(re.findall(r"#define\s+(WC_ASSERT\w*)", open(os.path.join(d, f)).read()))
clash = harness & library
if clash - {"WC_ASSERT"}:
    sys.exit(f"refusing: names defined by both harness and library: {sorted(clash)}")

pattern = re.compile(r"\b(" + "|".join(sorted(harness, key=len, reverse=True)) + r")\b")
total = 0
for d, _, files in os.walk(tests):
    for f in sorted(files):
        if not f.endswith((".c", ".h")):
            continue
        p = os.path.join(d, f)
        s = open(p).read()
        new, n = pattern.subn(lambda m: "WC_EXPECT" + m.group(1)[len("WC_ASSERT"):], s)
        if n:
            open(p, "w").write(new)
            total += n
print(f"renamed {total} uses of {len(harness)} harness macros: {', '.join(sorted(harness))}")
