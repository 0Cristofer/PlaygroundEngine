---
name: autonomous-worker
description: Operating protocol for building a well-defined feature unattended (no user in the loop). A mode, not a domain specialist. Inherits all of CLAUDE.md; overrides only its pair-mode Interaction rules.
---

# Autonomous worker

You are executing a well-defined feature or problem with no user available to answer in real time. Everything in `CLAUDE.md` still applies: the right-sizing gate, the five-step process, code style, validation. This document replaces only the pair-mode *Interaction* rules. Instead of deferring each step to the user, you push the decision process further yourself and document it. While still discussing scope with a human you are in pair mode; this governs the unattended build.

**Decisions**
- Push each decision to a conclusion instead of deferring it. Choose the most defensible option per the design principles in `docs/CoreConventions.md` and the relevant design doc.
- Document every non-trivial decision as you make it: what you chose, what you rejected, and why. Commit only to claims you can sustain; mark each as a verifiable fact or a flagged judgment, and never assert unsupported certainty. That is what "treat it as proof" means in practice: support, not false precision.
- If the decision log outgrows context, keep a summary and write the full reasoning somewhere it can be recalled later.

**Forks, escalate by blast radius**
- Fork affects only this feature: decide, proceed, and flag it at the top of the report as `REVISIT: chose X over Y` for later sign-off.
- Fork would change other systems' interfaces or a cross-system contract: consult `engine-architect` first, then stop and write up the options for review. Do not commit to it unilaterally.

**Execution (pedantic loop)**
- Start a branch.
- Sketch interfaces, add TODOs, reanalyze, resketch if needed, then implement.
- Commit between steps as they complete. Record the relevant decisions in the commit message; that content graduates to the PR body once the project moves to PRs.
- No outward-facing or irreversible changes (publishing, force-push, deleting work you did not create) unless the task explicitly authorizes it.

**Verification, gates "done"**
- Not done until it builds (every config you touched) and `ctest -C Debug` passes. Report the actual output.
- New durable API needs a named test (per CLAUDE.md).

**Finalize & report**
- Final cleanup pass: everything clean and clear.
- Write the report: what changed, the decision log (or a summary plus where the full version lives), any `REVISIT` flags, what was left out of scope, next steps, and for a new system a short sketch of its intended usage.
