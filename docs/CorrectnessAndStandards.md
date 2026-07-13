# Correctness & Standardization

How the engine verifies that code does what it claims, on both its human-facing and its
machine-facing side, and how style, comments, and documentation are kept uniform. Parent document:
[EngineDesign.md](../EngineDesign.md). Siblings: [CoreConventions.md](CoreConventions.md) (error
zones, memory seam, object model), [TestingSystem.md](TestingSystem.md) (test harness). Rationale
notes live in [CLAUDE.md](../CLAUDE.md).

**Status:** design only. Nothing here is built yet; this document is the agreed architecture and a
phased roadmap. Each phase (P0 and later) is implemented under its own approval. Where a toolchain
fact is stated as validated, it was checked against GCC 16.1.1 on this machine; everything else is a
design commitment.

## 1. Principle: the two-faced feature contract

A feature is not done until **both** of its outputs are pinned:

- **User-facing output**, what a caller observes: return values, rendered result, logged text. A
  named test exercises it.
- **Machine-facing effects**, what the change does underneath: allocations, copies and moves, type
  layout, reflection metadata, serialized bytes, exported module surface. These are normally
  invisible, so a change alters them silently and nobody, least of all an agent, is forced to
  notice.

The engine depends on correctness and determinism, and both human and agent authors carry wrong
assumptions about the second category. The remedy is to make machine-facing effects **observable and
pinned**, so any change to them surfaces as a diff that must be consciously accepted. That is the
mechanism by which everyone working on the project knows the effect of an edit.

This sits alongside two decisions already recorded elsewhere: the error zones in
[CoreConventions.md](CoreConventions.md#error-handling) (`std::expected` for expected failures,
contracts for programmer error, exceptions only in tooling and C#), and the guard-versus-assert
defensiveness stance (a check is either a guard with a real branch or an assertion that never
branches, never both). This document does not restate those; it builds on them.

## 2. Taxonomy of machine-facing outputs

What counts as a machine-facing output, and how each is observed and pinned. "Snapshot" means a
committed reference file diffed on every run (see [Section 3](#3-verification-toolkit)); "assertion"
means an explicit in-test `CHECK`.

| Output | Observed via | Pinned by |
|---|---|---|
| Type layout / ABI (size, alignment, field offsets, padding, trivial-copyability, standard-layout) | reflection + type traits | `static_assert` (primary, the numbers that matter); snapshot carries these only incidentally |
| Reflection metadata (registered types, fields, functions, facets, annotations, display names) | the reflection introspection surface | snapshot (as a toolchain-churn tripwire) |
| Allocation behavior (count, bytes, peak, arena) | tracking `memory_resource` | no-alloc scope + upper-bound budget (not exact counts) |
| Copy / move behavior (operation counts) | `Tracked<T>` probe | exact-count assertion (deterministic on our own types) |
| Serialization (byte format, round-trip identity) | serialization dumper | byte snapshot + round-trip property test |
| Module export surface | export dump | snapshot |
| Runtime invariants | contract `pre`/`post` | the contract itself, plus a test on rejection |
| Performance (nanoseconds per operation, budgets) | `PlaygroundBenchmark.Harness` | budget assertion |
| Determinism (per-tick world-state hash) | replay harness | golden trace (future) |

The reflection metadata row extends what already exists: the display-string characterization tests
in `PlaygroundTests/src/ReflectionTests.cpp` are the first instance of pinning a machine-facing
output, generalized here to the whole reflected shape.

The determinism row depends on the ECS and the `InputCommand` PODs, neither of which exists yet, so
it is deferred to a later phase.

## 3. Verification toolkit

The verification harness, the shared test-support library already anticipated in
[TestingSystem.md](TestingSystem.md#direction). The name follows the existing
`PlaygroundBenchmark.Harness`: a harness drives code and observes it, which is exactly the job here.
It lives inside `PlaygroundTests` until a second consumer forces extraction (per that document's
"do not split prematurely"). Building it by hand is expected: the bespoke toolchain (GCC 16, named
modules, `import std`, `-freflection`) rules out most off-the-shelf test tooling, and doctest plus
`PlaygroundBenchmark.Harness` are the precedent. The driving pieces are harnesses (`SnapshotHarness`);
the passive instruments (`Tracked<T>`, `TrackingResource`, the predicates) keep their own names and
the harness exercises them. It holds:

- **Memory.** `TrackingResource` wraps an upstream `std::pmr::memory_resource` and records
  allocation count, bytes, and peak. `NoAllocScope` installs a resource that fails the test on any
  allocation. Both ride the pmr seam in [CoreConventions.md](CoreConventions.md#ownership--memory):
  pattern-sensitive systems already accept a `memory_resource*`, so the test injects a tracking one.
  Once the ambient `operator new` funnel exists, `NoAllocScope` extends to all allocations, not only
  pmr-aware ones.
- **Probes.** `Tracked<T>` is a value type that counts its copies, moves, constructions, and
  destructions, for dropping into a container or a reflected struct to assert that an operation moves
  rather than copies.
- **Snapshots.** A generic `Snapshot(name, text)` compares against a committed reference file under
  `PlaygroundTests/snapshots/` and fails on any difference, with a `--bless` (or environment
  variable) flow to re-accept an intended change. A reflection-driven `DescribeType<T>()` produces the
  textual description, and a serialization dumper does the same for bytes. `DescribeType` **separates
  identity (names, kinds, field order) from layout numbers (size, alignment, offsets, padding) into
  distinct sections with canonical ordering**, so a compiler upgrade that only shifts padding touches
  one region and a reviewer can see at a glance that nothing structural changed.
- **Static-contract helpers.** `consteval` predicates over reflected types, `IsTriviallyReplicable`,
  `HasNoPadding`, `FitsBudget<T, N>`, `IsHandleLike`, `AllFieldsReflected`, with `static_assert`
  wrappers so a violated machine contract is a compile error at the point of definition. P0 delivers
  these as tools; *systematically applying* them as gates waits on the systems that define which types
  must satisfy them (the replication design, and the handle/C#-boundary identity scheme still open in
  [CoreConventions.md](CoreConventions.md)).

Snapshots are the backbone (they catch changes nobody thought to assert); targeted `static_assert`s
and assertions pin the specific numbers that matter (this size, trivially copyable, no allocation
here, exactly one move). Machine snapshots are **characterization tripwires**, the same framing
[TestingSystem.md](TestingSystem.md) already uses for the display-string tests: a diff on a GCC
upgrade is expected and blessed, not an alarm, because reflected display strings are
implementation-defined and offsets move across compiler versions. Goldens are single-toolchain
artifacts, meaningful only pinned to the project's GCC 16 build.

The memory probes (`TrackingResource`, `NoAllocScope`, `Tracked<T>`) are built when their first
consumer lands (a `memory_resource*`-taking system, or serialization), not in P0: there is no hot
path to guard yet, so building them now is ahead of need.

## 4. Runtime contracts and the assertion facility

C++26 contracts are the engine's assertion mechanism and **retire `<cassert>`**. Plain `assert` is
the weakest option available: coarse `NDEBUG` on/off that does not map to the `PGE_DEV` /
`PGE_RELEASE` zones, a bare `abort()` with no logging, no structured diagnostic, no handler seam, and
untestable because it kills the test runner. Validated on GCC 16.1.1 with `-fcontracts`: `pre`,
`post`, and `contract_assert` parse and enforce, and a violation prints function, location, and
predicate then terminates via `abort` without unwinding.

- **`contract_assert`** replaces in-body `assert`; **`pre`/`post`** replace the assert-at-entry and
  assert-at-exit idiom and become part of the declaration, visible to callers.
- **Semantics by zone.** Enforce in `PGE_DEV`, observe in a telemetry build, ignore in shipping,
  selected with `-fcontract-evaluation-semantic=`. Because contracts terminate without unwinding,
  they hold under `-fno-exceptions`, which the runtime zone requires.
- **The value over `assert` lives in the violation handler.** The contracts proposal defines a
  **standard, replaceable contract-violation handler**; the engine supplies one that routes to
  `PGE_LOG` / spdlog with the assertion kind, source location, and predicate text, and applies the
  fatal-or-telemetry policy for the build zone. This is the language's own customization point, not
  an engine idiom. Validate the exact GCC 16 handler signature with a throwaway compile before
  relying on it (contracts are experimental).
- **Test seam.** An enforced violation terminates and cannot be caught by doctest. The observe
  semantic is not a fix: it records the violation and then *continues into the body with the bad
  input*, which is the undefined behavior the precondition existed to prevent. So the **test build
  only** installs a handler that **throws**, and tests assert rejection with `CHECK_THROWS`. This is
  legal because tests are tooling-zone code compiled exceptions-on
  ([TestingSystem.md](TestingSystem.md), Considerations); it stops the body from running and
  integrates natively with doctest. The throwing handler exists only in the test build; runtime code
  keeps the terminating enforce/observe handler under `-fno-exceptions`.
- **Minimal residue.** Only what contracts do not cover: `std::unreachable()` for genuinely
  unreachable paths, and an always-on `PGE_VERIFY` for conditions that must be checked even in
  release. `PGE_VERIFY` is needed because the contract evaluation semantic is chosen **globally per
  build** (`-fcontract-evaluation-semantic=`), so there is no standard way to ignore contracts in
  shipping *except this one*; an always-on check is the only way to keep a specific condition live in
  a build that otherwise ignores contracts. It logs and aborts on failure. It is an assertion, not a
  guard, because it has no meaningful recovery branch: a false result means a bug, and the program
  stops. There is no parallel macro assertion system; the language provides contracts, and the engine
  adds the handler plus this thin residue, honoring the [CLAUDE.md](../CLAUDE.md) rule to use the
  language mechanism rather than an engine-specific one.
- **Guard versus assert.** Contracts are non-branching by construction: there is no boolean to test,
  so the `if (ensure(...))` pattern is not expressible. That is deliberate. A guard is a real `if`
  with a real `else`; an assertion never branches.
- **Where contracts go first.** The `TypedRef` op-table boundary
  (`PlaygroundEngine/src/Reflection/TypedRef.cppm`, `FieldInfo.cppm`), where a wrong `Type` or `Data`
  assumption is silent undefined behavior today, and `Engine::Run` boot ordering. The durable
  `assert` sites migrate to contracts: `Engine.cpp:64` and the reflection facet op-table invariants
  (`StringFacet.cppm:34`, `SequenceFacet.cppm:48/56/62`). Leave `GameObject.cppm`'s asserts alone:
  that model is a placeholder slated for deletion ([CoreConventions.md](CoreConventions.md), Object
  Model), so migrating them is churn on code that is going away.
- **Where they must not go.** The `std::expected` / validation surface (`FieldInfo`'s `FieldError`).
  A dynamic type mismatch during generic reflected access is an expected runtime condition the caller
  handles, not a programmer error.

## 5. Static enforcement and content validation

Two compile-time and data-time layers distinct from runtime contracts:

- **Static machine contracts.** The `consteval` predicates from [Section 3](#3-verification-toolkit)
  applied as `static_assert` gates on types: a replicable POD stays trivially copyable and
  standard-layout, a handle stays `int64`-convertible for the C# boundary, every reflected field is
  itself reflected. A violation is a compile error, so this class of machine contract costs nothing
  at runtime and cannot regress silently. The predicates are buildable now; which types they are
  pointed at is decided by the replication and C#-boundary designs (still open), so early use is
  opt-in per type rather than a blanket sweep.
- **Content validation.** Designer-authored data is invalid in normal operation (a reference left
  unset mid-edit is ordinary, not a bug), so it is neither a contract nor an error. A validation pass
  walks reflected fields, checks constraint annotations (for example `[[Required]]`), and produces
  `ValidationDiagnostic{ severity, message, target, field }` records that the editor surfaces and the
  cook step can block on. At runtime a missing asset reference resolves to a visible error placeholder
  rather than crashing. This layer is reflection-annotation driven and arrives with the editor.

## 6. Standardization: style, comments, documentation

### Style enforcement and the clang question

Semantic clang tooling cannot run here, and the reason is more than modules. P2996 reflection
(`^^T`, `std::meta`) is not in mainline clang (only the `clang-p2996` fork), and `-freflection` is a
GCC-only flag clang rejects, so the reflection translation units will not compile under stock clang,
and clang-tidy needs a compile. `.clang-tidy` therefore stays the **naming source of truth**,
enforced by the IDE; a `compile_commands.json` already exists for when mainline clang catches up, at
which point this is re-evaluated.

`clang-format` is a separate tool that needs no compile: it is lexer-based and recent versions handle
`export module` and `import`. The P0 spike is **done and positive** (clang-format 22.1.2): it does
**not mangle** any bleeding-edge syntax, `^^T`, splicers `[:...:]`, `std::meta`, and
`export module ... : partition;` all survive formatting intact (exit 0, no errors), and a
project-matching config (Microsoft base, tabs, 150-column, Allman via `BreakBeforeBraces: Custom`,
`IndentRequiresClause: false`, `FixNamespaceComments: false`, `SplitEmptyFunction: false`) took a
representative file to **zero churn**. Residual churn on dense files is a few more tunable knobs
(`BreakTemplateDeclarations`, `AllowShortFunctionsOnASingleLine: Inline`, `BinPackParameters: false`),
not divergence. **Decision: adopt clang-format** for layout enforcement. Adoption is its own step:
commit a tuned `.clang-format`, do a one-time whole-repo reflow that becomes the new baseline, and
point ReSharper at the same `.clang-format` so the IDE and the pipeline agree. The textual lint below
stays for the rules clang-format does not cover (em-dash ban, comment length, naming).

### Textual lint

The guaranteed-portable floor, a script enforcing the regex-checkable rules from
[CLAUDE.md](../CLAUDE.md) and `.editorconfig`: the em-dash ban, tabs not spaces, final newline, no
trailing whitespace, maximum line length, an Allman brace heuristic, comment placement (no floating
comment above a block), the 3-line comment cap (below), and an approximate abbreviated-name
heuristic. Full naming and abbreviation checks need an AST that clang cannot build here, so the lint
approximates them.

### Comment length

No comment is longer than **3 lines**. Anything that needs more explanation belongs in a `docs/`
document, not inline. This reinforces the existing minimal-comments stance (comments only where logic
is genuinely complex), is enforced by the textual lint, and is added to the code-style rules.

### Documentation

Public modules and APIs get doc comments once their surface stabilizes; `docs/` remains the
architecture record and the home for anything that outgrows the 3-line cap. A doc-coverage check
**reports** a newly exported symbol with no doc; because exported surfaces churn constantly in the
exploration phase and the stated policy is that doc comments come once a surface stabilizes, this is
a report, not a gate, until a module is marked stable, at which point it gates that module only. Each
feature's test file carries a short machine-contract note listing the outputs it pins, so the
intended effects are discoverable next to the tests that hold them.

## 7. Enforcement fabric: a local pipeline that mirrors cloud

A single canonical pipeline, a `scripts/verify.sh` script and/or a CMake `verify` target, with
ordered, independently invokable stages:

```
configure → build (warnings as errors) → textual lint → static contracts (compile)
          → unit + characterization tests → snapshots → sanitizers (asan/ubsan/tsan)
          → benchmarks / budgets
```

**Local equals cloud.** The stage contract is defined once and run locally now. A future
`.github/workflows/ci.yml` is a thin wrapper that calls the same stages, and an optional `Dockerfile`
reproduces the source-built GCC 16 so the local runner and the eventual cloud runner are identical.
The point is to build the tooling now and defer only the cloud cost. New CMake presets `linux-asan`
and `linux-tsan` provide the sanitizer configurations, but running the full modular build (`import
std`, global `-freflection`) under ASan/TSan is unproven on this bespoke setup, so, like the
clang-format spike, sanitizers get a validation step (confirm they configure, link, and do not
false-positive on the module runtime) before entering the hard pipeline. The benchmarks/budgets stage
is **advisory**: perf under WSL and containers is flaky, so it measures and reports, and gates only on
large regressions with wide tolerance, if at all.

**The one hard gate is merge into `main`.** The full pipeline must be green to integrate a branch
into `main`; `main` is always green. Everything before that boundary is advisory, so the branch is a
free workspace:

| Point | Runs | Blocks? |
|---|---|---|
| Claude Code `PostToolUse` on source Edit/Write | fast textual lint on the edited file | advisory (fix-forward) |
| Claude Code `Stop` / `SubagentStop` | build + affected tests, reported | advisory |
| git `pre-commit` / feature-branch push | fast lint + affected tests, reported | advisory |
| **merge into `main`** | **full pipeline** (build, lint, static contracts, tests, snapshots, sanitizers) | **hard-block** |

This is what dissolves the collision with `.claude/agents/autonomous-worker.md`: sketch-with-TODO
commits and mid-feature red stops live on the branch, which is allowed to be red; only integration
into `main` is gated, and that is a deliberate action, not an autonomous mid-run stop. Stages run
cheap-to-expensive and fail fast, scoped to changed targets where possible. Locally the gate is a
`main`-integration script (or hook) that runs the pipeline before the merge is allowed to land;
cloud maps it directly to branch protection with a required check on `main`.

Because branches may be red, the merge gate needs no per-stop escape hatch. The only residual case is
pre-existing breakage on `main` itself blocking an otherwise-clean merge; that is handled by keeping
`main` green as an invariant (a red `main` is an incident to fix first, not to bypass).

## 8. Feature-completion contract

The workflow integration, extending step 5 (Finalize) of the working method in
[CLAUDE.md](../CLAUDE.md). A feature is complete when:

1. A named test pins the user-facing output.
2. Snapshots and assertions pin every applicable machine-facing output from
   [Section 2](#2-taxonomy-of-machine-facing-outputs).
3. Static contracts are declared for the type's machine properties.
4. Runtime contracts guard the invariants that exist.
5. Style and documentation lint pass.
6. The verify pipeline is green.

This may later be surfaced as a `/verify-feature` skill. It is the step that makes an author, human
or agent, declare and pin the effects of the work.

## 9. Phased roadmap

- **P0, foundations.** The reflection-serving tools that generalize what `ReflectionTests.cpp` does
  by hand today: the snapshot harness and `DescribeType`, the static-contract *predicates*, the
  clang-format spike, the textual lint script, the `verify.sh` skeleton, and the cheapest hooks. The
  memory probes are deliberately not here (no consumer yet).
- **P1.** Runtime contracts (a `linux-dev` plus `-fcontracts` preset, the violation handler, the
  throwing test-seam handler, the `PGE_VERIFY` residue), retiring `<cassert>` and migrating the
  durable `assert` sites (`Engine.cpp:64`, the reflection facets), the sanitizer spike then presets,
  and the doc-coverage report.
- **P2.** The memory probes (`TrackingResource`, `NoAllocScope`, `Tracked<T>`) arriving with their
  first consumer, serialization snapshots and round-trip property tests (with serialization), and the
  content validation layer (with annotations and the editor).
- **P3.** The determinism replay harness (with the ECS and `InputCommand`), the containerized
  local-equals-cloud CI, and the GitHub Actions wrapper.

## 10. Open questions and risks

- **Keeping `main` green.** Gating only at merge into `main` resolves the autonomous-wedging concern
  (branches may be red), so the residual risk is a red `main` blocking clean merges; the invariant is
  that a red `main` is an incident to fix immediately, not a state to bypass.
- **Snapshot churn.** Golden files require re-blessing discipline; a large or surprising snapshot diff
  in review is a signal, not a chore to wave through.
- **Textual lint is heuristic.** Without a clang AST, naming and abbreviation checks are approximate;
  the IDE and review remain the backstop until mainline clang can parse these units.
- **CI container maintenance.** Reproducing a source-built GCC 16 in an image is real upkeep, weighed
  against the value of local-equals-cloud parity.
- **Contracts are experimental.** GCC 16's contract support is new; validate each non-trivial use
  with a throwaway compile before relying on it, per the working method.
- **The build-mode matrix must coexist.** The set {enforce test build with a throwing handler
  (exceptions-on) × observe telemetry build × ignore shipping × `-fno-exceptions` runtime} needs to be
  validated as a whole, not per-mode, since the test and runtime builds use different handlers and
  exception settings.
- **Goldens are single-toolchain artifacts.** They are meaningful only pinned to the project's GCC 16
  build; a cross-toolchain diff is not a regression, and nothing should treat it as one.
