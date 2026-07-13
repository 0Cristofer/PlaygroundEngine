# Correctness & Standardization

How the engine verifies that code does what it claims, on both its human-facing and its
machine-facing side, and how style, comments, and documentation are kept uniform. Parent document:
[EngineDesign.md](../EngineDesign.md). Siblings: [CoreConventions.md](CoreConventions.md) (error
zones, memory seam, object model), [TestingSystem.md](TestingSystem.md) (test harness). Rationale
notes live in [CLAUDE.md](../CLAUDE.md).

**Status:** P0 built; later phases are design. This document is the agreed architecture and a phased
roadmap; each phase is implemented under its own approval. Where a toolchain fact is stated as
validated, it was checked against GCC 16.1.1 on this machine; everything past P0 is a design
commitment. What P0 delivered is marked **[built]** inline and summarized in
[Section 9](#9-phased-roadmap).

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

The verification pillars are four, not three. A snapshot records what the code *did* (a
characterization tripwire); a static or runtime contract asserts a truth the compiler or a single
execution can check; and **property-based testing** asserts a truth that must hold for *all* inputs,
not the one an author happened to pick. That last is the closest match to this document's own
premise, "reason about what the code actually does, not an assumed shape," because a golden pins
whatever behavior occurred while a property pins the invariant the behavior must satisfy. The two are
complementary: a snapshot answers "did the observable output change?", a property answers "is this
still true for inputs nobody chose?". Property testing enters with its first real invariant
(serialization round-trip); the shared enabler is a reflection-driven instance generator (see
[Section 3](#3-verification-toolkit)).

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
- **Snapshots [built].** `CheckSnapshot(name, actual)` compares against a committed reference file
  under `PlaygroundTests/snapshots/` and fails on any difference, reporting the first differing line;
  the `PGE_BLESS` environment variable re-writes the golden to re-accept an intended change. A missing
  golden without blessing is a failure, not a silent pass. A reflection-driven `DescribeType<T>()`
  produces the textual description (a serialization dumper for bytes is future). `DescribeType`
  **separates identity (names, kinds, field order) from layout numbers (size, alignment, offsets,
  padding) into distinct sections with canonical ordering**, so a compiler upgrade that only shifts
  padding touches one region and a reviewer can see at a glance that nothing structural changed. It
  renders the type kind through the reflection system's own `ToString(TypeKind)` rather than a
  hand-maintained switch, so a new kind cannot drift out of sync with the describer. These live in
  module `PlaygroundTests.SnapshotHarness`, namespace `PgE::Snapshot` (the domain name, matching
  `PgE::Benchmark`, not a generic `Harness`). The identity section prefers the author-written
  identifier over the implementation-defined display name (`SnapshotHarness.cppm` `Label`); that
  discipline is extended so display strings stay out of the diffed text wherever an identifier
  suffices, quarantining the toolchain-dependent churn that would otherwise mask a real structural
  diff across many goldens at once on a compiler upgrade.
- **Harness self-verification [built].** The describer sources every layout number from the reflection system
  (`traits.Size`, `field.GetByteOffset()`), which is the riskiest code in the tree, so a wrong trait
  would be faithfully pinned and blessed into a golden. Two cheap tests anchor the harness to the
  language independently of the reflection path: a **ground-truth cross-check** asserting the
  reflected size, alignment, offsets, and trivial-copyability equal the language's own `sizeof`,
  `alignof`, `offsetof`, and `std::is_trivially_copyable_v` for known standard-layout types; and a
  **discrimination test** proving `DescribeType` text actually changes when a field is reordered or
  added, so the snapshot is shown to catch what it exists to catch, not assumed to. A full mutation
  engine (per-type fault injection) is deferred; its structure-aware form falls out of the reflected
  instance generator later (below) at near-zero marginal cost.
- **Coverage manifest.** Per-type pinning is opt-in, so a *new* reflected type is never forced into
  the mechanism and the coverage of the pinning is itself unpinned. The fix reuses the machinery
  already built: a single aggregate characterization golden that lists every reflected type and
  whether its shape is pinned. Adding a type diffs the manifest, which must be consciously blessed,
  that is the forcing function, at the cost of one more golden rather than a bespoke coverage gate.
  There is no runtime type registry (`TypeOf<T>()` is a lazy per-type meta-instantiation; a keyed
  registry is future work noted in `TypeInfo.cpp`), so the enumeration source is compile-time
  namespace reflection (`members_of(^^PgE)` and nested namespaces), whose cross-module reachability is
  exactly the fragile GCC-16 area and **must be spiked before the instrument is designed**. Default-on
  as a *report* first; the escalation to a per-module hard gate reuses the doc-coverage stability
  escalation in [Section 6](#6-standardization-style-comments-documentation), gating only a module
  marked stable.
- **Property-based testing.** A seeded loop over generated inputs asserting an invariant, with the
  seed reported on failure and a shrink seam for minimizing a counterexample. The shared enabler for
  this, structure-aware fuzzing, and the eventual structure-aware mutation test is one primitive: a
  reflection-driven instance generator `Arbitrary<T>` that walks reflected fields and recurses. It is
  built once, with serialization round-trip (`deserialize . serialize == identity`) as its first real
  consumer, because that is where properties become the acceptance criterion and where untrusted bytes
  make fuzzing mandatory. Hand-written generators bootstrap the loop before the reflected generator
  lands.
- **Static-contract helpers [built, partial].** `consteval` predicates over reflected types with
  `static_assert` wrappers so a violated machine contract is a compile error at the point of
  definition. P0 shipped `IsTriviallyReplicable`, `HasNoPadding` (reflection-walked, summing
  `offset_of`/`size_of` and rejecting any bit offset or gap), and `FitsBudget<T, MaxBytes>` in module
  `PlaygroundEngine.Reflection.Contracts` (re-exported from `PlaygroundEngine.Reflection`). The
  boundary-specific `IsHandleLike` and `AllFieldsReflected` are deferred: *systematically applying*
  predicates as gates waits on the systems that define which types must satisfy them (the replication
  design, and the handle/C#-boundary identity scheme still open in
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

**Adopt contracts directly; no interim assertion macro.** An engine-specific `PGE_ASSERT` shim over
`contract_assert` was considered and rejected. It would be exactly the kind of parallel construct
carried alongside the real mechanism that this project's no-legacy principle forbids, and hedging on
contracts specifically, while the project already rides `import std`, global `-freflection`, and
`std::meta`, is an inconsistent risk posture: the edge is accepted everywhere else. The tooling risk
is real and accepted; if GCC's contract semantics or handler signature shift, the fix lands at the
contract site, not behind a shim. What is *not* deferred is the **violation handler**: it is engine
policy regardless of how the trap lowers, it is the sole source of the value contracts add over
`assert`, and it is built first, with `contract_assert`/`pre`/`post` adopted directly on top. The
throwing test-seam handler (below) is needed identically either way. Each non-trivial contract use is
still validated with a throwaway compile before it is relied on, per the working method.

- **`contract_assert`** replaces in-body `assert`; **`pre`/`post`** replace the assert-at-entry and
  assert-at-exit idiom and become part of the declaration, visible to callers. Contracts carry no
  message string, so where the predicate does not by itself convey the invariant, a brief comment
  states what it guards or how a caller avoids it (a [CLAUDE.md](../CLAUDE.md) code-style rule), so the
  reasoning the old `assert` string held is not lost.
- **Semantics by zone.** Enforce in `PGE_DEV`, observe in a telemetry build, ignore in shipping,
  selected with `-fcontract-evaluation-semantic=`. The semantic rides the build config rather than a
  standalone knob (it is a whole-program property, uniform across every TU including the `import std`
  target, so it cannot be a per-target option): the root `CMakeLists.txt` sets `enforce` for the dev
  configs (Debug, RelWithDebInfo) and `ignore` for Release (shipping) via `$<CONFIG>` [built]. Because
  contracts terminate without unwinding, they hold under `-fno-exceptions`, which the runtime zone
  requires; the telemetry `observe` config and the `-fno-exceptions` runtime get their own presets
  (still open).
- **The value over `assert` lives in the violation handler.** The contracts proposal defines a
  **standard, replaceable contract-violation handler**; the engine supplies one that routes to
  `PGE_LOG` / spdlog with the assertion kind, source location, and predicate text, and applies the
  fatal-or-telemetry policy for the build zone. This is the language's own customization point, not
  an engine idiom. Validate the exact GCC 16 handler signature with a throwaway compile before
  relying on it (contracts are experimental). **The engine owns this, not the game.** Since the engine
  owns `main`, it defines the global-linkage handler in its entry-point translation unit (`main.cpp`),
  which is co-linked into any engine-driven executable and never pulled into targets with their own
  `main` (tests, benchmarks). The reusable formatting/zone *policy* lives in `PlaygroundEngine.Diagnostics`;
  the handler is a thin dispatch to it. A game needing custom behavior would attach through an
  engine-provided hook, not by defining the replaceable function itself, that stays an engine concern.
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
  stops. `PGE_VERIFY` is the only assertion macro; there is no other parallel mechanism. The language
  provides contracts, the engine adds the handler plus this thin residue, honoring the
  [CLAUDE.md](../CLAUDE.md) rule to use the language mechanism rather than an engine-specific one.
- **Guard versus assert, and no `ensure`.** Contracts are non-branching by construction: there is no
  boolean to test, so the Unreal-style `if (ensure(...))` pattern (check, report, then continue on the
  bad value) is not expressible, and no such branching macro is added. That is deliberate and it is
  the whole point of the split. A condition with a real recovery path is a **guard**: a real `if`
  returning a `std::expected` error, ordinary control flow, not an assertion. A condition with no
  recovery is an **assertion** that aborts. There is no third category, so there is no place for a
  reporting-but-continuing `ensure`; the report-and-continue behavior that Unreal folds into `ensure`
  is instead the contract `observe` semantic at the telemetry build, chosen globally, needing no macro.
- **Where contracts go first.** The `TypedRef` op-table boundary
  (`PlaygroundEngine/src/Reflection/Core/TypedRef.cppm`, `Core/FieldInfo.cppm`), where a wrong `Type`
  or `Data` assumption is silent undefined behavior today, and `Engine::Run` boot ordering. The durable
  `assert` sites migrate to contracts: `Engine.cpp:64` and the reflection facet op-table invariants
  (`Builtins/StringFacet.cppm:32`, `Builtins/SequenceFacet.cppm:49/57/63`). Leave `GameObject.cppm`'s asserts alone:
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

Semantic clang tooling cannot run here, and the reason is more than modules. The dividing line is
that `clang-format` is lexer-only (it never parses), while `clang-tidy` needs a full parse and a
semantic AST, so the two are not a package deal and one working says nothing about the other. This was
**verified, not assumed**, against the installed clang 22.1.2 including `clang-tidy-22` itself:
`-freflection` is rejected as an *unknown argument*, `--help-hidden` exposes *no* reflection or P2996
flag at all, and `^^int` is a hard parse error (clang reads the `^` as its Blocks extension). Run on a
real reflection module, `clang-tidy-22` fails even earlier, at `import std` (`module 'std' not
found`), the BMI blocker below. P2996 reflection (`^^T`, `std::meta`,
splicers) is not in mainline clang (only the `clang-p2996` fork), so any reflection translation unit
fails at the parse `clang-tidy` depends on. A second, independent blocker: the module BMIs are
GCC-produced and clang cannot consume them, so `clang-tidy` would have to rebuild the whole module
graph (including `import std`) its own way regardless. `.clang-tidy` therefore stays the **naming
source of truth**, enforced by the IDE; a `compile_commands.json` already exists for when mainline
clang gains P2996 *and* GCC-compatible modules, at which point this is re-checked with the same
throwaway probe.

`clang-format` is a separate tool that needs no compile: it is lexer-based and recent versions handle
`export module` and `import`. The P0 spike was **positive** (clang-format 22.1.2): it does **not
mangle** any bleeding-edge syntax, `^^T`, splicers `[:...:]`, `std::meta`, and
`export module ... : partition;` all survive formatting intact. **clang-format is now adopted
[built].** The committed `.clang-format` (Microsoft base, tabs, 150-column, Allman via
`BreakBeforeBraces: Custom`, `IndentRequiresClause: false`, `FixNamespaceComments: false`,
`SplitEmptyFunction: false`) drove a one-time whole-repo reflow that is the current baseline. Two
choices there are load-bearing and were tightened past the spike:

- **`InsertBraces: true`** makes the [CLAUDE.md](../CLAUDE.md) "always braces" rule mechanical:
  every `if`/`else`/loop gets braces even for a single-statement body, and the formatter adds them.
- **`AllowShortFunctionsOnASingleLine: None`** (the spike had used `Inline`): no function body is
  collapsed onto its signature line. Every block opens on the next line, so a one-line accessor reads
  the same as any other function. This is a deliberate uniformity call.
- **`AllowShortCaseLabelsOnASingleLine: false`**: a `case` body always goes on the next line, never
  collapsed onto the label. Same uniformity call as the function-body one, so a one-line case reads
  like any other.

Point ReSharper at the same `.clang-format` so the IDE and the pipeline agree (an IDE setting, not
yet done). The textual lint below stays for the rules clang-format does not cover (em-dash ban,
comment length, naming).

### Textual lint [built]

The guaranteed-portable floor: `scripts/lint.sh` enforces the regex-checkable rules from
[CLAUDE.md](../CLAUDE.md) and `.editorconfig`. Shipped checks: the em-dash ban (U+2014), trailing
whitespace (which also catches whitespace on otherwise-blank lines), final newline, and the 3-line
comment cap. With no arguments it audits every tracked C++, Markdown, and CMake file; with file
arguments it lints only those, so the merge gate can pass just the files changed versus `main` and
grandfather existing code. CMake files (`CMakeLists.txt`, `*.cmake`) get the em-dash / whitespace /
final-newline hygiene, since those rules are not C++-specific and CMake was previously an unlinted
blind spot; the comment cap stays C++ only (it scans `//` and `/* */`, not CMake `#`). Run-of-directive
comment blocks (`// ReSharper disable`, `NOLINT`, `clang-format`) are exempt from the comment cap. The
rules that need an AST clang cannot build here (naming, abbreviation) are left to the IDE, not
approximated, to avoid false positives. A CMake *formatter* is now adopted [built]: **gersemi**
(`.gersemirc`: tabs and 150 columns to match `.editorconfig` / `.clang-format`) drove a one-time
reflow of every `CMakeLists.txt`, and the `cmakeformat` verify stage runs `gersemi --check`, mirroring
the clang-format stage for C++. Its opinionated `if(...)` (no space) becomes the CMake baseline, the
same kind of uniformity call as the clang-format choices above; the unknown-command warning for
doctest's `doctest_discover_tests` helper is informational and does not fail the stage.

### Comment length [built]

No comment is longer than **3 lines**. Anything that needs more explanation belongs in a `docs/`
document, not inline. This reinforces the existing minimal-comments stance (comments only where logic
is genuinely complex) and is enforced by `scripts/lint.sh`. Enforcing it across the existing tree
surfaced the knowledge-dense reflection comments; rather than delete them, the load-bearing material
(the `TypeOfMeta` recursion knot, lazy `TypeReference`, the GCC-16 mangling-collision workaround, the
validated `std::meta` patterns) was migrated to [ReflectionInternals.md](ReflectionInternals.md), and
the inline comments now point there. Migrating outsized comments to a doc, not condensing away the
reasoning, is the intended response when this cap bites.

### Documentation

Public modules and APIs get doc comments once their surface stabilizes; `docs/` remains the
architecture record and the home for anything that outgrows the 3-line cap. A doc-coverage check
**reports** a newly exported symbol with no doc; because exported surfaces churn constantly in the
exploration phase and the stated policy is that doc comments come once a surface stabilizes, this is
a report, not a gate, until a module is marked stable, at which point it gates that module only. Each
feature's test file carries a short machine-contract note listing the outputs it pins, so the
intended effects are discoverable next to the tests that hold them.

## 7. Enforcement fabric: a local pipeline that mirrors cloud

**Enforced, not expected.** A procedure that depends on an author *choosing* to follow it is not
enforced, it is hoped for, and an agent will follow what a gate mechanically requires while skipping
what a document merely asks. So the governing rule of this section is: anything that *can* be
mechanically enforced *must* be, and the residue that genuinely cannot be is minimized and made
visible rather than left to discipline. This is why opt-in pinning is upgraded to a forced diff (the
coverage manifest in [Section 3](#3-verification-toolkit)), why re-blessing is surfaced as a counted
gate category (below) rather than trusted, and why the feature-completion contract
([Section 8](#8-feature-completion-contract)) is a machine-checked list, not a checklist an author
self-certifies. Where a rule cannot yet be mechanized (naming without an AST clang, doc coverage on a
churning surface), it is a *report*, explicitly labeled as not-yet-enforced, so the gap is known and
tracked toward enforcement, never quietly accepted as done.

A single canonical pipeline, `scripts/verify.sh`, with ordered, independently invokable stages
(pass a stage name to run one, or nothing to run all). The full designed sequence:

```
configure → build (warnings as errors; Debug / RelWithDebInfo / Release) → format check (clang-format)
          → textual lint → shellcheck → static contracts (compile)
          → unit + characterization tests → snapshots → property tests → coverage (gcov, advisory)
          → sanitizers (asan / ubsan) → contract-mode matrix (enforce / observe / ignore / -fno-exceptions)
          → fuzz targets (short) → benchmarks / budgets (advisory)
```

Every stage above runs on this one machine; that is the point of local-equals-cloud. Beyond the P0
four, the added stages are all local and toolchain-only: a **format check** (`clang-format
--dry-run -Werror`, lexer-based, catches drift since the one-time reflow), **shellcheck** on
`scripts/*.sh`, a **Debug/RelWithDebInfo/Release build matrix** (config-dependent breakage, and the
paths where contracts compile out), **gcov coverage** (advisory), and the **contract-mode matrix**
that discharges the build-mode risk in [Section 10](#10-open-questions-and-risks). TSan is deferred
until real concurrency exists (the ECS/networking), where it earns its keep.

**Deep static analysis has no viable tool on this toolchain (validated, negative result).** The plan
was `gcc -fanalyzer` as the path-sensitive analyzer filling the hole clang-tidy cannot (Section 6). A
spike on GCC 16.1.1 killed it: `-fanalyzer` silently emits **no diagnostics** for any translation
unit that `import`s a named module, `import std` included. Confirmed both in the real engine build
(an injected null-dereference, `new`-leak, and `malloc`-leak in an engine module unit all passed
`-fanalyzer -Werror`) and standalone (the identical defects are caught with the `import` removed, and
importing even a trivial user module suppresses them). Every first-party TU imports `std`, so the
analyzer sees none of our code; wiring the stage would be a slow, memory-hungry build (the analyzer
also explodes over `std::meta` instantiation) that catches nothing. So both deep-static-analysis
options are currently unavailable, clang-tidy by a parse blocker and `-fanalyzer` by this module
blocker. For now the path-sensitive-bug layer is carried by the runtime sanitizers (ASan/UBSan,
[Section 3](#3-verification-toolkit)), which run the real code and are module-agnostic.

**`-fanalyzer` stays a wanted revisit item, not a closed one**, because static and dynamic analysis
are complementary rather than redundant. Sanitizers only see paths a test executes; with line coverage
at ~21%, most code, including the cold `std::expected` error branches, is never reached at runtime. A
static analyzer explores those paths symbolically with no triggering input, at compile time, which is
exactly the blind spot sanitizers structurally cannot cover (its cost is false positives, which
sanitizers avoid). Its value here is highest while coverage is low and shrinks but never vanishes as
the suite grows. **Revisit trigger:** re-run the throwaway probe (an `import std` TU with an injected
null-deref under `-fanalyzer -Werror`) on each GCC upgrade; when a module-importing TU produces the
diagnostic, wire the stage (scoped off the reflection-dense TUs, which also explode the analyzer). The
GCC analyzer's module support is experimental, so this is a "watch upstream," not a "never." The same
re-check applies to clang-tidy if mainline clang gains P2996 and GCC-compatible modules (Section 6).

**Built so far:** `configure → build → format → cmakeformat → lint → shellcheck → test → matrix → sanitizers → coverage`. Configure always
runs before build (a source addition to a `CMakeLists.txt` is otherwise silently skipped by an
incremental `--build`). The **format check** (`clang-format --dry-run -Werror`), **shellcheck**, and
**build matrix** stages are now wired [built]; the tool-bearing ones detect their tool across
environments (the versioned `clang-format-22` here versus an unversioned container install; shellcheck
on `PATH` or under `~/.local/bin`). Two format-stage carve-outs were forced by the spike:
clang-format-22 misreads a bare `identifier && identifier` inside a contract `pre(...)` as an
rvalue-ref, so contract predicates are written with explicit comparisons
(`_app != nullptr && _world != nullptr`), which read better anyway; and the `PlaygroundReflection`
`std::meta` scratch headers are exempt (their `^^` operators trip clang-format's Objective-C guesser,
and they keep intentional hand-alignment as throwaway exploration). The build matrix adds only the
RelWithDebInfo and Release configs (Debug is already built and feeds the tests) and runs last as the
heaviest stage. Static contracts are `static_assert`s, so they are enforced *within* the build stage
rather than as a separate step, and the snapshot checks currently run *within* the test stage (they are
doctest cases). The **sanitizers** stage is now wired [built]: ASan + UBSan on the whole program via
the `linux-asan` preset, running the suite; its parallelism is memory-capped because ASan roughly
doubles per-TU memory. The **coverage** stage is now wired [built] and **advisory**: the `linux-coverage`
preset instruments with `--coverage`, the suite runs to emit the counters, and `gcovr` reports a
summary filtered to first-party source (using the toolchain's own `gcov`; a mismatched system `gcov`
cannot read GCC 16 data). It measures and prints, it does not gate on a threshold. The
contract-mode-matrix and benchmark stages are not built (they need presets and spikes still in P1);
`gcc -fanalyzer` is not a stage at all, ruled out above by the module blocker. The current pipeline
runs green: build clean, format clean, lint clean, shellcheck clean, 84/84 tests, matrix
(Debug/RelWithDebInfo/Release) clean, 84/84 again under ASan+UBSan with zero sanitizer diagnostics, and
a coverage report (currently line 21% of first-party source, honestly low: the reflection-focused suite
does not exercise the windowing or GameObject-skeleton code).

**Local equals cloud.** The stage contract is defined once and run locally now. A future
`.github/workflows/ci.yml` is a thin wrapper that calls the same stages, and an optional `Dockerfile`
reproduces the source-built GCC 16 so the local runner and the eventual cloud runner are identical.
The point is to build the tooling now and defer only the cloud cost. The `linux-asan` preset carries
ASan + UBSan and the spike is **done [built]**: the full modular build (`import std`, global
`-freflection`) configures, links, and runs the whole suite under both sanitizers with **zero false
positives on the module runtime**, and an injected heap-buffer-overflow is caught and fails the stage.
Instrumenting the std module and deps too (global, not first-party-scoped) is deliberate: it keeps
libstdc++ containers instrumented so ASan raises no container-overflow false positive. Unlike a static
analyzer, sanitizers are codegen instrumentation and so are not blinded by module imports. TSan waits
for real concurrency (the ECS/networking); its `linux-tsan` preset is deferred with it. The benchmarks/budgets stage
is **advisory**: perf under WSL and containers is flaky, so it measures and reports, and gates only on
large regressions with wide tolerance, if at all.

**What a server adds that a machine cannot.** Because local equals cloud, a CI server does not run
*different* work, it runs the same `verify.sh`. It exists only for the four properties one developer
box structurally lacks: (1) **unbypassable enforcement**, the local `pre-commit` merge gate is honest
but skippable (`--no-verify`, a manual merge), so "`main` is always green" as an *invariant* needs
server-side branch protection with required checks; (2) a **merge queue** that tests the *combination*
of two individually-green branches, catching merge skew nothing local can see; (3) **environments this
box does not have**, the cross-compiler / cross-OS matrix (Windows/MSVC, clang, macOS), mostly
*deferred* here since the project is GCC-16-only, and each such environment needs its own blessed
golden set because [Section 10](#10-open-questions-and-risks) goldens are single-toolchain; and (4)
**always-on, scaled, historical** work, the clean-room from-scratch build in the `Dockerfile`
(proving it builds on nothing, not on a hand-built `~/gcc-16` under WSL), sustained fuzzing and
determinism soaks, matrix sharding, and perf/coverage *trends* over time. Everything else, including
short fuzz runs and single-machine sanitizer passes, is local. Given the single-toolchain reality
today, the server's near-term value is items (1) and (4) only; the rest arrives with a second target
or a second contributor.

**The one hard gate is merge into `main`.** The full pipeline must be green to integrate a branch
into `main`; `main` is always green. Everything before that boundary is advisory, so the branch is a
free workspace:

| Point | Runs | Blocks? |
|---|---|---|
| Claude Code `PostToolUse` on source Edit/Write | fast textual lint on the edited file | advisory (fix-forward) |
| Claude Code `Stop` / `SubagentStop` | build + affected tests, reported | advisory |
| git `pre-commit` / feature-branch push | fast lint + affected tests, reported | advisory |
| **merge into `main`** | **full pipeline** (build, lint, static contracts, tests, snapshots, sanitizers) | **hard-block** |

The merge gate reports **re-blessed goldens as a distinct, counted category** (a diff over
`PlaygroundTests/snapshots/`), so "this merge re-blesses N goldens" is a surfaced line item rather
than a silent `PGE_BLESS`. Blessing is the harness's only integrity backstop, and it is unguarded
precisely on the autonomous-worker path, where no human is in the loop and the merge gate is the sole
interception; a mass-churn re-bless on a toolchain upgrade is exactly when a real regression hides, so
the count is not a nicety but the enforcement of conscious acceptance the whole snapshot mechanism
rests on.

The git hooks are wired [built]: tracked under `scripts/hooks/` and activated automatically at CMake
configure time (the root `CMakeLists.txt` points `core.hooksPath` there, so they are version-controlled
rather than untracked in `.git/hooks`, and no manual per-clone step is needed; `scripts/setup-hooks.sh`
is the manual equivalent). Git has no clone-time hook by design, so folding activation into the
configure step every developer runs is the automatic path. `pre-commit` lints the changed files on a branch commit. The hard
gate (full `verify.sh`) lives in **`pre-merge-commit`**, because since git 2.24 a clean automatic
merge commit runs that hook, not `pre-commit`; `pre-commit` carries the same gate only for the other
path, a conflicted merge that git finishes as a manual commit (`MERGE_HEAD` still present). `pre-push`
mirrors the gate for when a remote exists. The Claude Code hooks (`PostToolUse` lint on edit, `Stop`
build plus tests) are wired in `.claude/settings.json` as advisory reports. A `main` merge must be
`--no-ff` so a merge commit exists for the gate to run on: a fast-forward creates no commit and would
slip past the hooks entirely.

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
   [Section 2](#2-taxonomy-of-machine-facing-outputs); a new reflected type appears in the coverage
   manifest, pinned or consciously exempted.
3. Static contracts are declared for the type's machine properties.
4. Runtime contracts guard the invariants that exist.
5. Properties are stated for the invariants that must hold over all inputs (round-trip, and any other
   universally-quantified truth), not only the hand-picked cases.
6. Style and documentation lint pass.
7. The verify pipeline is green.

Consistent with [Section 7](#7-enforcement-fabric-a-local-pipeline-that-mirrors-cloud), this is a
machine-checked contract, not a self-certified checklist: each item above is enforced by the pipeline
where it can be (the manifest diff, the static and runtime contracts, lint, the green gate), so an
author, human or agent, cannot mark a feature done while an enforceable item is unmet. This may later
be surfaced as a `/verify-feature` skill. It is the step that makes an author declare and pin the
effects of the work.

## 9. Phased roadmap

- **P0, foundations [done].** Delivered: the snapshot harness and `DescribeType`
  (`PlaygroundTests.SnapshotHarness`, `PgE::Snapshot`); the static-contract predicates
  `IsTriviallyReplicable` / `HasNoPadding` / `FitsBudget` (`PlaygroundEngine.Reflection.Contracts`);
  the clang-format spike **and** adoption plus whole-repo reflow (`InsertBraces`, no single-line
  bodies); the textual lint script (`scripts/lint.sh`, em-dash / whitespace / final-newline / 3-line
  comment cap); and the `verify.sh` pipeline (configure/build/lint/test). Deferred out of P0: the
  memory probes (no consumer yet), the `IsHandleLike` / `AllFieldsReflected` predicates (boundary
  designs still open), and the hooks (P1 wiring).
- **P1.** *Harness self-verification* [built]: the reflection ground-truth cross-check (reflected
  numbers equal `sizeof`/`alignof`/`offsetof`/`is_trivially_copyable_v`), the snapshot discrimination
  test (a reorder or added field changes the `DescribeType` body), and the display-string quarantine
  (the describer diffs on the stable identifier, not the implementation-defined display name) all
  shipped in `PlaygroundTests/src/HarnessSelfVerificationTests.cpp`. Runtime contracts [built]: `-fcontracts` with the enforce semantic is global (validated
  against the modular `import std` build), the engine's `LogContractViolation` policy routes through
  spdlog and the engine installs the runtime handler at its own entry point (`main.cpp`, co-linked
  into any engine-driven executable, so the game stays unaware of it), the test build installs a
  throwing seam (`PlaygroundTests.ContractSeam`) that tests assert against with `CHECK_THROWS_AS`, `PGE_VERIFY`
  is the always-on residue, and `<cassert>` is retired at the migrated sites (`Engine::StartRun` now a
  `pre`, the reflection facet op-tables now `pre`). Adopted `contract_assert`/`pre`/`post` directly
  with no interim macro. The per-zone semantic split rides `$<CONFIG>` [built]: dev configs enforce,
  Release ignores, so the build matrix already validates the enforce and ignore modes; still open are
  the telemetry `observe` config and the `-fno-exceptions` runtime, each needing its own preset. The
  property-based-testing loop primitive (hand-written generators to start). The ASan+UBSan sanitizer
  spike, preset (`linux-asan`), and `sanitizers` verify stage are [built] (whole-program, zero false
  positives on the module runtime, injected-bug discrimination confirmed); the fuzzing-instrumentation
  spike and the `linux-tsan` preset stay deferred (TSan waits on real concurrency). The
  namespace-enumeration spike gating
  the coverage manifest, then the manifest as a default-on report. The doc-coverage report. New local
  pipeline stages, all toolchain-only: the `clang-format --dry-run -Werror` drift check [built],
  `shellcheck` on `scripts/*.sh` [built], the Debug/RelWithDebInfo/Release build matrix [built], and the
  `gersemi` CMake-format check [built], and the `gcov` coverage stage [built, advisory] (`linux-coverage`
  preset, `gcovr`-summarized, filtered to first-party source); still open are the remaining
  contract-mode-matrix legs (`observe` / `-fno-exceptions`) once their presets exist. The
  `gcc -fanalyzer` static-analysis stage was spiked and dropped: its analyzer emits nothing for a TU
  that imports a module (Section 7), so it is blind to our code. The enforcement fabric on
  top of `verify.sh` is wired [built]: the tracked git hooks (branch lint plus the `--no-ff`
  `main`-merge full-pipeline gate and its re-blessed-goldens category) and the advisory Claude Code
  hooks (`PostToolUse` lint, `Stop`/`SubagentStop` verify).
- **P2.** The reflected-instance generator `Arbitrary<T>` (the shared keystone for properties,
  fuzzing, and structure-aware mutation), arriving with serialization as its first consumer. The
  memory probes (`TrackingResource`, `NoAllocScope`, `Tracked<T>`). Serialization byte snapshots and
  round-trip property tests, the deserializer fuzz target, and the optional structure-aware mutation
  pass (all on the generator). The content validation layer (with annotations and the editor).
- **P3.** The determinism replay harness (with the ECS and `InputCommand`), the network-packet fuzz
  target (reusing the generator), the containerized local-equals-cloud CI, and the GitHub Actions
  wrapper. The coverage manifest's per-module hard gate lands here as modules are marked stable.

## 10. Open questions and risks

- **Keeping `main` green.** Gating only at merge into `main` resolves the autonomous-wedging concern
  (branches may be red), so the residual risk is a red `main` blocking clean merges; the invariant is
  that a red `main` is an incident to fix immediately, not a state to bypass.
- **Snapshot churn.** Golden files require re-blessing discipline; a large or surprising snapshot diff
  in review is a signal, not a chore to wave through. The counted re-bless gate category
  ([Section 7](#7-enforcement-fabric-a-local-pipeline-that-mirrors-cloud)) and display-string
  quarantine ([Section 3](#3-verification-toolkit)) are the structural backstops that keep this from
  resting on discipline alone.
- **Coverage-manifest feasibility.** The manifest depends on compile-time namespace enumeration
  (`members_of(^^PgE)` across modules); there is no runtime type registry to iterate. Cross-module
  reachability of type declarations is a fragile GCC-16 area, so the enumeration is spiked in
  `PlaygroundReflection/` before the instrument is designed, and the manifest is a report until that
  is proven. Default-deny gating on a churning reflected surface is deliberately avoided: it would
  train authors to reflex-bless, recreating the blessing risk above; the manifest forces a *visible*
  decision instead, and hard gating waits for per-module stability.
- **No deep static analysis is available (a wanted revisit, not a closed door).** Both candidates are
  blocked on this toolchain: clang-tidy cannot parse these units (Section 6), and `gcc -fanalyzer`
  emits nothing for a TU that imports a module, so it never sees our code (Section 7). The
  path-sensitive-bug layer rests on the runtime sanitizers *for now*, but static analysis stays worth
  restoring because it covers the unexecuted paths sanitizers structurally cannot (most of the code
  while coverage is low). Re-run each tool's throwaway probe on every GCC/clang upgrade; wire the stage
  the moment a module-importing TU produces a diagnostic. This is a "watch upstream," not a "never."
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

## 11. Prior art and industry grounding

None of this is invented. Each mechanism here is an instance of an established discipline, and this
section names the precedent so a reader can check the reasoning rather than take it on the document's
word, which is the same standard the document holds code to. There is no single "correctness harness"
in industry; correctness is a layered composition of specialized tools, and the sections above pick
one instance of each layer suited to this toolchain.

| Mechanism here | Discipline | Representative prior art |
|---|---|---|
| Snapshots, `DescribeType` ([Section 3](#3-verification-toolkit)) | Characterization / approval testing | Feathers' characterization tests; ApprovalTests; Jest snapshots (also the rubber-stamp cautionary tale); the Rust compiler's `.stderr` UI tests with `--bless`; `insta`'s interactive `cargo insta review` |
| Coverage manifest ([Section 3](#3-verification-toolkit)) | Enumerated-surface gates (default-deny) | Roslyn `PublicAPI.Shipped.txt`; Go's checked-in `api/*.txt`; `cargo-public-api` |
| Type-layout / ABI pinning ([Section 2](#2-taxonomy-of-machine-facing-outputs)) | ABI diffing | libabigail (`abidiff`); `swift-api-digester` |
| Static contracts, `consteval` predicates ([Section 3](#3-verification-toolkit)) | Rule-based invariants (not snapshots) | `cargo-semver-checks` |
| Harness self-verification ([Section 3](#3-verification-toolkit)) | Mutation testing | PITest; Stryker; `cargo-mutants`; Petrović and Ivanković's diff-based approach at Google [4] |
| Runtime contracts, handler, zones ([Section 4](#4-runtime-contracts-and-the-assertion-facility)) | Design by Contract | Eiffel (Meyer); Ada 2012 / SPARK; Unreal `check`/`verify`/`ensure`; Chromium / abseil `DCHECK`/`CHECK` |
| `PGE_VERIFY` (always-on) ([Section 4](#4-runtime-contracts-and-the-assertion-facility)) | Always-on assertion | Unreal `verify`; abseil `CHECK` |
| The assertion practice itself ([Section 4](#4-runtime-contracts-and-the-assertion-facility)) | Empirical assertion-density evidence | Kudrjavets, Nagappan, Ball, higher assertion density correlates with lower fault density [1] |
| Property-based testing ([Section 3](#3-verification-toolkit)) | QuickCheck lineage | QuickCheck; Hypothesis; `proptest`; RapidCheck |
| Fuzzing ([Section 3](#3-verification-toolkit), P2/P3) | Coverage-guided / structure-aware fuzzing | libFuzzer; AFL++; OSS-Fuzz; libprotobuf-mutator |
| Determinism replay, per-tick hash ([Section 2](#2-taxonomy-of-machine-facing-outputs), P3) | Lockstep desync detection | Bettner and Terrano, deterministic sim makes a recorded game an exact repro [3]; GGPO rollback; Factorio desync reports |
| Memory / copy-move probes ([Section 3](#3-verification-toolkit), P2) | Instrumented test types, no-alloc scopes | Chromium `AssertNoAllocationScope`; Hinnant-style counting test types |
| Merge gate, `main` always green ([Section 7](#7-enforcement-fabric-a-local-pipeline-that-mirrors-cloud)) | The "not rocket science" rule | Ben Elliston's rule, applied by Hoare's `bors` for Rust [2]; modern merge queues |
| Local equals cloud ([Section 7](#7-enforcement-fabric-a-local-pipeline-that-mirrors-cloud)) | Hermetic builds | Bazel; Nix |

Two patterns across the strongest examples shaped the amendments in this document. First, the mature
tools are **default-deny enumerated gates with reviewed acceptance** (Roslyn `PublicAPI`, Go's `api/`,
`insta`'s per-snapshot review), which is why opt-in pinning was upgraded to the coverage manifest and
blessing was made a counted, surfaced gate category rather than a silent env var. Industry learned the
opt-in-snapshot lesson (rubber-stamped updates, snapshot rot) the hard way. Second, **property-based
testing and fuzzing assert what must be *true* over unchosen inputs**, the complement to a snapshot
that only records what happened; they are the layer closest to this document's own premise, so
property testing was promoted to a pillar and fuzzing scheduled where untrusted bytes make it
mandatory (serialization, networking). The one place this engine is deliberately stricter than a
cited precedent is the guard-versus-assert split: Unreal's `ensure` folds check-and-continue into one
construct, and [Section 4](#4-runtime-contracts-and-the-assertion-facility) rejects exactly that.

References:

1. G. Kudrjavets, N. Nagappan, T. Ball. "Assessing the Relationship between Software Assertions and
   Faults: An Empirical Investigation." ISSRE 2006 (Microsoft Research TR-2006-54).
   <https://www.microsoft.com/en-us/research/wp-content/uploads/2016/02/tr-2006-54.pdf>
2. Rust's `bors` applying Ben Elliston's "not rocket science" rule (automatically maintain a
   repository that always passes its tests, avoiding merge skew). Graydon Hoare, circa 2013.
3. P. Bettner, M. Terrano. "1500 Archers on a 28.8: Network Programming in Age of Empires and
   Beyond." GDC 2001. <https://www.gamedeveloper.com/programming/1500-archers-on-a-28-8-network-programming-in-age-of-empires-and-beyond>
4. G. Petrović, M. Ivanković. "State of Mutation Testing at Google." ICSE-SEIP 2018.
   <https://research.google/pubs/state-of-mutation-testing-at-google/>
