#!/usr/bin/env bash
# The verification pipeline: ordered, fail-fast stages that mirror what a future cloud CI runs.
# Run every stage, or one by name: scripts/verify.sh
# [configure|build|format|cmakeformat|lint|shellcheck|test|matrix|sanitizers].
# A cloud CI job is a thin wrapper that calls these same stages, so local and cloud stay identical.
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root" || exit 1

# clang-format ships here as the versioned clang-format-22; a container would install the unversioned
# name. Prefer whatever the environment exposes so local and cloud run the same lexer-based check.
find_clang_format() {
	local candidate
	for candidate in clang-format clang-format-22; do
		if command -v "$candidate" >/dev/null 2>&1; then
			printf '%s' "$candidate"
			return 0
		fi
	done
	return 1
}

# The checker is not always on PATH on this box (installed under ~/.local/bin); a container puts it
# on PATH. Check both so the stage runs identically in either place.
find_shellcheck() {
	if command -v shellcheck >/dev/null 2>&1; then
		printf 'shellcheck'
		return 0
	fi
	if [ -x "$HOME/.local/bin/shellcheck" ]; then
		printf '%s' "$HOME/.local/bin/shellcheck"
		return 0
	fi
	return 1
}

# gersemi is the CMake formatter (installed under ~/.local/bin here, on PATH in a container).
find_gersemi() {
	if command -v gersemi >/dev/null 2>&1; then
		printf 'gersemi'
		return 0
	fi
	if [ -x "$HOME/.local/bin/gersemi" ]; then
		printf '%s' "$HOME/.local/bin/gersemi"
		return 0
	fi
	return 1
}

# Configure is always run before build: a CMakeLists source addition is otherwise silently skipped
# until the next reconfigure.
stage_configure() { cmake --preset linux; }

# Building the tests also compiles the static-contract assertions, so a broken machine contract fails
# here as a build error.
stage_build() { cmake --build --preset linux-debug; }

# Lexer-based drift check against the committed .clang-format; catches any file that fell out of the
# one-time whole-repo reflow. The PlaygroundReflection std::meta scratch headers are exempt: they keep
# their hand-aligned exploration formatting, and clang-format's guesser reads their `^^` reflection
# operators as Objective-C. C++26 contract predicates avoid bare `identifier && identifier`, which
# clang-format misreads as an rvalue-ref (see docs/CorrectnessAndStandards.md).
stage_format() {
	local formatter
	if ! formatter="$(find_clang_format)"; then
		printf 'verify: clang-format not found (need clang-format or clang-format-22)\n'
		return 1
	fi
	git ls-files '*.cpp' '*.cppm' '*.h' '*.hpp' ':(exclude)PlaygroundReflection/src/*.h' | xargs "$formatter" --dry-run --Werror
}

# CMake formatter drift check (gersemi, config in .gersemirc), mirroring the clang-format stage for
# C++. --check exits non-zero if a file would be reformatted; the unknown-command warning it prints for
# doctest's helper macro is informational and does not fail the stage.
stage_cmakeformat() {
	local formatter
	if ! formatter="$(find_gersemi)"; then
		printf 'verify: gersemi not found (pip install gersemi, or drop it in ~/.local/bin)\n'
		return 1
	fi
	git ls-files '*CMakeLists.txt' '*.cmake' | xargs "$formatter" --check
}

stage_lint() { "$root/scripts/lint.sh"; }

stage_shellcheck() {
	local checker
	if ! checker="$(find_shellcheck)"; then
		printf 'verify: shellcheck not found (install it or drop a static binary in ~/.local/bin)\n'
		return 1
	fi
	git ls-files 'scripts/*.sh' 'scripts/hooks/*' | xargs "$checker"
}

stage_test() { (cd build/linux && ctest -C Debug --output-on-failure); }

# Config-dependent breakage: the RelWithDebInfo and Release paths (PGE_RELEASE, and where the enforce
# contract semantic compiles out) are not exercised by the Debug build+test above. Debug is already
# built by the build stage, so the matrix adds only the other two configs. Heaviest stage, runs last.
stage_matrix() {
	cmake --build --preset linux-dev && cmake --build --preset linux-release
}

# ASan roughly doubles per-TU memory, so a full-core build of the modular import-std TUs can exhaust
# memory. Cap by available memory (~2 GB per TU) and never exceed the core count.
sanitizer_jobs() {
	local cores available_kb by_memory
	cores="$(nproc 2>/dev/null || echo 1)"
	available_kb="$(awk '/^MemAvailable:/ { print $2 }' /proc/meminfo 2>/dev/null || echo 0)"
	by_memory=$((available_kb / 1024 / 1024 / 2))
	if [ "$by_memory" -lt 1 ]; then
		by_memory=1
	fi
	if [ "$by_memory" -lt "$cores" ]; then
		printf '%s' "$by_memory"
	else
		printf '%s' "$cores"
	fi
}

# ASan + UBSan on the whole program. Instrumenting the std module and deps too keeps libstdc++
# containers instrumented, so ASan raises no container-overflow false positives on the module runtime.
# Runtime codegen instrumentation, so unlike a static analyzer it is not blinded by module imports. A
# sanitizer error aborts the offending test and fails the stage. Parallelism is memory-capped. See
# docs/CorrectnessAndStandards.md Section 7.
stage_sanitizers() {
	cmake --preset linux-asan && cmake --build --preset linux-asan -j "$(sanitizer_jobs)" &&
		(cd build/linux-asan && ctest -C Debug --output-on-failure)
}

order=(configure build format cmakeformat lint shellcheck test matrix sanitizers)

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
