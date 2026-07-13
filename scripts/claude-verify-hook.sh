#!/usr/bin/env bash
# Claude Code Stop/SubagentStop hook. When tracked source changed this turn, runs the verify pipeline
# and advisorily reports a failure via systemMessage. Never blocks the stop (always exit 0), and skips
# entirely on turns that touched no source, so conversational turns stay instant.
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root" || exit 0

if ! git status --porcelain 2>/dev/null | grep -qE '\.(cpp|cppm|h|hpp)$|CMakeLists\.txt'; then
	exit 0
fi

log=/tmp/pge_stop_verify.log
if ./scripts/verify.sh >"$log" 2>&1; then
	exit 0
fi

printf '{"systemMessage":"PGE verify failed on stop (source changed); see %s"}\n' "$log"
exit 0
