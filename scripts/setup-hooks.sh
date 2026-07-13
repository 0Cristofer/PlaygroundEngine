#!/usr/bin/env bash
# One-time activation of the tracked git hooks. Points core.hooksPath at scripts/hooks so the hooks
# are version-controlled rather than living untracked in .git/hooks. Run once per clone.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
git -C "$root" config core.hooksPath scripts/hooks

printf 'git hooks active: core.hooksPath -> scripts/hooks\n'
