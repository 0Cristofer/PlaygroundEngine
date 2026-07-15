#!/usr/bin/env bash
# Claude Code Stop/SubagentStop hook. When tracked source changed this turn, runs the fast verify
# stages (configure, build, test on the debug tree) and advisorily reports a failure via systemMessage.
# Never blocks the stop (always exit 0), and skips entirely on turns that touched no source, so
# conversational turns stay instant.
#
# Deliberately not the full pipeline: the heavy stages (matrix, sanitizers, coverage) each rebuild a
# separate tree from scratch, which is far too much to pay on every stop in a dynamic conversation.
# Those run as the hard gate in scripts/hooks/pre-merge-commit when integrating into main, so main
# still gets the full check. This hook is only the incremental "did I break the build or a test".
# Format and lint are already covered per-edit by claude-lint-hook.sh (PostToolUse on Edit/Write).
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root" || exit 0

if ! git status --porcelain 2>/dev/null | grep -qE '\.(cpp|cppm|h|hpp)$|CMakeLists\.txt'; then
	exit 0
fi

log=/tmp/pge_stop_verify.log
: >"$log"
for stage in configure build test; do
	if ! ./scripts/verify.sh "$stage" >>"$log" 2>&1; then
		printf '{"systemMessage":"PGE %s failed on stop (source changed); see %s"}\n' "$stage" "$log"
		exit 0
	fi
done

exit 0
