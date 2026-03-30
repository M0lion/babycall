---
name: implement-feature
description: >
  Structured feature implementation pipeline with exploration, design review,
  implementation, and testing phases. Use whenever the user asks to implement,
  build, add, or create a new feature, endpoint, component, or capability.
  Also use for significant refactors or changes that touch multiple files.
---

# Implement Feature: $ARGUMENTS

You are orchestrating a structured feature implementation. Follow this pipeline strictly.
Do NOT skip phases. Do NOT combine phases. Each phase uses a subagent to keep context clean.

## Phase 1: Explore

Spawn a subagent (subagent_type: explorer) to investigate the codebase.
The subagent should:
- Identify files, modules, and patterns relevant to the feature
- Understand existing conventions (naming, structure, error handling)
- Find related tests and how they're structured
- Return a concise summary — NOT raw file contents

Wait for the exploration summary before proceeding.

## Phase 2: Design

Using the exploration summary, draft a design proposal that includes:
- What files will be created or modified
- Key interfaces, data structures, or API shapes
- How it fits into existing patterns
- Any trade-offs or open questions

**STOP HERE. Present the design to the user and ask for feedback.**

Do not proceed to implementation until the user explicitly approves or adjusts the design.
If the user requests changes, revise the design and present it again.
Ask about anything that is ambiguous or has multiple reasonable approaches.

## Phase 3: Implement

Once the user approves the design, spawn a subagent (subagent_type: implementer) to write the code.
Pass it:
- The approved design
- The exploration summary
- Any specific instructions from the user

The implementer should follow the approved design precisely. If it encounters
something unexpected that requires a design change, it should note it in its
output rather than making the decision itself.

Review the implementation output. If the implementer flagged any issues,
raise them with the user before proceeding.

## Phase 4: Test

Spawn a subagent (subagent_type: tester) to verify the implementation.
The tester should:
- Run existing tests to check for regressions
- Write new tests for the implemented feature if a test framework exists
- Try to build/compile the project if applicable
- Report results back

If tests fail, spawn another implementer subagent to fix the issues,
then re-run the tester. Repeat until tests pass or the issue needs user input.

## Phase 5: Summary

Present the user with:
- What was implemented (files created/modified)
- Test results
- Any caveats or follow-up items
