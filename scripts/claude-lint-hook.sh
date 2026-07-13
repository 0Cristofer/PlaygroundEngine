#!/usr/bin/env bash
# Claude Code PostToolUse hook (Edit|Write). Lints the edited file and surfaces any violation as
# advisory fix-forward feedback: exit 2 feeds the output back to the model, the edit itself is not
# blocked. Silent on a clean file. Reads the hook payload JSON on stdin.
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
file="$(jq -r '.tool_input.file_path // .tool_response.filePath // empty')"

[ -n "$file" ] || exit 0

if ! output="$("$root/scripts/lint.sh" "$file" 2>&1)"; then
	printf '%s\n' "$output" >&2
	exit 2
fi

exit 0
