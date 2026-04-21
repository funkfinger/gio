# Stories

Work in this repo is organised as user stories. Each story is a file in this directory, numbered sequentially. Stories are satisfied by code and, where the code is pure logic, by tests first (TDD).

## Format

Each story is a Markdown file named `NNN-kebab-case-title.md` with this structure:

```markdown
# Story NNN: Title

**As a** <role>
**I want** <capability>
**So that** <benefit>

## Acceptance Criteria
- [ ] Checkbox per criterion. Concrete, verifiable, not aspirational.

## Notes
Anything the implementer needs: constraints, context, links to related stories, deferred edge cases.

## Depends on
- Story NNN

## Status
not started | in progress | done (PR #N)
```

## Roles

- **Developer** — someone working in the repo (toolchain, scaffolding, libraries).
- **Bench engineer** — someone with the hardware on the desk doing bring-up, calibration, measurement.
- **Patcher** — the end user playing the module in a Eurorack rack.

Most early stories are Developer or Bench engineer. Patcher stories land once the module makes sound.

## Workflow

1. Pick the lowest-numbered story with status `not started` whose dependencies are done.
2. Move to `in progress`, open a feature branch.
3. For pure-logic work: write failing tests matching the acceptance criteria, then code to pass.
4. For HAL / bench work: build the smallest thing that satisfies the criteria, verify on bench, record measurements in the story or linked doc.
5. Tick acceptance criteria as they land.
6. Open PR. On merge, mark story `done (PR #N)`.

## Writing new stories

New stories can be added at any time — during planning sessions, or when an existing story surfaces work that deserves its own scope. Keep each story small enough to land in one PR. If a story grows past that, split it.
