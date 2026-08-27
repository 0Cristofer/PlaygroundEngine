#!/usr/bin/env bash
# Claude Code PostToolUse hook (Edit|Write). Lints the edited file and surfaces any violation as
# advisory fix-forward feedback: exit 2 feeds the output back to the model, the edit itself is not
# blocked. Silent on a clean file. Reads the hook payload JSON on stdin.
#
# Also re-states the comment rules when an edit adds comment lines to a C++ file. That check is a
# trigger, not a judge: it decides only that comments were added, never whether one is justified,
# so it has no false-positive verdict to argue with. It exists because CLAUDE.md is read once at
# session start, while the comment gets written hundreds of tool calls later.
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
payload="$(cat)"
file="$(jq -r '.tool_input.file_path // .tool_response.filePath // empty' <<<"$payload")"

[ -n "$file" ] || exit 0

# Only lint files inside the repo. Edits elsewhere (the memory index, scratchpad) are out of the
# project lint's scope, and the memory index uses a harness-prescribed em-dash format of its own.
abs="$(realpath -m -- "$file" 2>/dev/null || echo "$file")"
case "$abs" in "$root"/*) ;; *) exit 0 ;; esac

CountCommentLines() {
	printf '%s\n' "$1" |
		grep -vE '^[[:space:]]*// *(ReSharper|NOLINT|clang-format)' |
		grep -cE '^[[:space:]]*(//|/\*|\*[^/])' || true
}

CommentsWereAdded() {
	case "$abs" in
		*.cpp | *.cppm | *.h | *.hpp) ;;
		*) return 1 ;;
	esac

	local added removed
	added="$(jq -r '[.tool_input.content, .tool_input.new_string, (.tool_input.edits // [] | .[].new_string)] | map(select(. != null)) | join("\n")' <<<"$payload")"
	removed="$(jq -r '[.tool_input.old_string, (.tool_input.edits // [] | .[].old_string)] | map(select(. != null)) | join("\n")' <<<"$payload")"

	[ "$(CountCommentLines "$added")" -gt "$(CountCommentLines "$removed")" ]
}

status=0

if ! output="$("$root/scripts/lint.sh" "$file" 2>&1)"; then
	printf '%s\n' "$output" >&2
	status=2
fi

if CommentsWereAdded; then
	cat >&2 <<'RULES'
comment rules (CLAUDE.md, Code style): this edit adds comments. For each one:
  - Delete it unless its absence would let a future editor make a silently wrong change.
  - "Why we chose this" belongs in docs/ or the commit message; rationale for a reviewer belongs in your response.
  - Banned shapes: "this is X, not Y, so ...", "we do A rather than B", "otherwise it would ...", restating the code.
  - What survives is one terse present-tense sentence; a second sentence means a docs/ entry and a pointer to it.
  - Tests comment nothing, the test name carries the claim.
Before keeping one, try a better name, an extracted local, a named constant, a contract, or a test.
RULES
	status=2
fi

exit "$status"
