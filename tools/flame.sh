#!/usr/bin/env bash

# Profile one bench with perf and produce a flamegraph SVG.
# Usage: scripts/flame.sh [bench-filter] [build-dir]
#   scripts/flame.sh hashmap.get_string
#   scripts/flame.sh 'hashmap.*' build
set -euo pipefail

FILTER="${1:-*}"
BUILD="${2:-build}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
BENCH="$BUILD/bench"
OUT="tests/flame_out"
FG="${FLAMEGRAPH_DIR:-$ROOT/tests/FlameGraph}"
SAFE="$(echo "$FILTER" | tr -c 'A-Za-z0-9_.' '_')"

command -v perf >/dev/null || { echo "perf not found (install linux-tools)"; exit 1; }
[[ -x "$BENCH" ]] || { echo "$BENCH not found. Build first."; exit 1; }

[[ -f "$FG/flamegraph.pl" ]] || { echo "flamegraph.pl not found in $FG"; exit 1; }

mkdir -p "$OUT"
DATA="$(mktemp)"
trap 'rm -f "$DATA"' EXIT

# DWARF unwinding works without -fno-omit-frame-pointer, but needs -g symbols.
# 4096 byte stack dump keeps perf.data small enough for a short bench.
perf record -F 2999 --call-graph dwarf,4096 -o "$DATA" -- \
  "$BENCH" --filter="$FILTER"

perf script -i "$DATA" \
  | perl "$FG/stackcollapse-perf.pl" \
  | perl "$FG/flamegraph.pl" --title "bench: $FILTER" > "$OUT/flame.$SAFE.svg"

echo "Wrote $OUT/flame.$SAFE.svg"
