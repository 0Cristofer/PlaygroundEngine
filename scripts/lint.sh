#!/usr/bin/env bash
# Textual lint: the portable floor for the standardization rules clang-format does not own.
# Covers the em-dash ban, file hygiene (trailing whitespace, final newline), and the 3-line
# comment cap. Naming and abbreviation checks need an AST clang cannot build here and are left
# to the IDE for now (see docs/CorrectnessAndStandards.md).
#
# With no arguments, lints every tracked source and Markdown file (a full audit). With file
# arguments, lints only those (the merge gate passes the files changed versus main, so existing
# code is grandfathered and only new violations block).
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

if [ "$#" -gt 0 ]; then
	files=("$@")
else
	mapfile -t files < <(git ls-files '*.cpp' '*.cppm' '*.h' '*.hpp' '*.md')
fi

violations=0
flag() { printf 'lint: %s\n' "$1"; violations=$((violations + 1)); }

is_source() { case "$1" in *.cpp | *.cppm | *.h | *.hpp) return 0 ;; *) return 1 ;; esac; }
is_prose() { case "$1" in *.cpp | *.cppm | *.h | *.hpp | *.md) return 0 ;; *) return 1 ;; esac; }

for file in "${files[@]}"; do
	[ -f "$file" ] || continue

	if is_prose "$file"; then
		# Em-dash reads as machine-written prose.
		while IFS=: read -r number _; do
			flag "$file:$number em-dash (U+2014); use commas, parentheses, or separate sentences"
		done < <(grep -nP '\x{2014}' "$file" || true)

		# Final newline.
		if [ -s "$file" ] && [ "$(tail -c1 "$file" | wc -l)" -eq 0 ]; then
			flag "$file: missing final newline"
		fi
	fi

	if is_source "$file"; then
		# Trailing whitespace. Markdown is exempt: a trailing double-space is a hard line break there.
		while IFS=: read -r number _; do
			flag "$file:$number trailing whitespace"
		done < <(grep -nP '[ \t]+$' "$file" || true)

		# Comment length: a longer explanation belongs in a docs/ document, not inline.
		while IFS= read -r finding; do
			flag "$finding"
		done < <(awk '
			function emit() { if (run > 3) { print FILENAME ":" start " comment is " run " lines (cap is 3); move it to docs/" } }
			{
				text = $0; sub(/^[ \t]+/, "", text)
				if (inblock) {
					run++
					if (text ~ /\*\//) { emit(); run = 0; inblock = 0 }
					next
				}
				# Tool directives (ReSharper, NOLINT, clang-format) are not prose; they break the run.
				if (text ~ /^\/\// && text !~ /^\/\/ *(ReSharper|NOLINT|clang-format)/) { if (run == 0) { start = FNR } run++; next }
				emit(); run = 0
				if (text ~ /^\/\*/ && text !~ /\*\//) { inblock = 1; start = FNR; run = 1 }
			}
			END { emit() }
		' "$file")
	fi
done

if [ "$violations" -gt 0 ]; then
	printf '\nlint: %d violation(s)\n' "$violations"
	exit 1
fi

printf 'lint: clean\n'
