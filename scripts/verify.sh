#!/usr/bin/env bash
# The verification pipeline: ordered, fail-fast stages that mirror what a future cloud CI runs.
# Run every stage, or one by name: scripts/verify.sh [configure|build|lint|test].
# A cloud CI job is a thin wrapper that calls these same stages, so local and cloud stay identical.
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

# Configure is always run before build: a CMakeLists source addition is otherwise silently skipped
# until the next reconfigure.
stage_configure() { cmake --preset linux; }

# Building the tests also compiles the static-contract assertions, so a broken machine contract fails
# here as a build error.
stage_build() { cmake --build --preset linux-debug; }

stage_lint() { "$root/scripts/lint.sh"; }

stage_test() { (cd build/linux && ctest -C Debug --output-on-failure); }

order=(configure build lint test)

run_stage() {
	local name="$1"
	printf '\n=== verify: %s ===\n' "$name"
	if ! "stage_$name"; then
		printf 'verify: stage %s FAILED\n' "$name"
		exit 1
	fi
}

if [ "$#" -gt 0 ]; then
	run_stage "$1"
else
	for name in "${order[@]}"; do
		run_stage "$name"
	done
	printf '\nverify: all stages passed\n'
fi
