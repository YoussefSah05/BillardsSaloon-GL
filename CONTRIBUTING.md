# Contributing

How work flows through this repository. The short version: small topic
branches off `main`, Conventional Commits, one merge commit per finished piece
of work, a tag per milestone.

## Branches

- **`main`** is always buildable, tested and playable. Nobody commits to it
  directly; it only receives merges.
- **Topic branches** start from the latest `main` and live only until merged:

  | Prefix | For |
  |--------|-----|
  | `feat/` | new player-visible behaviour or systems |
  | `fix/` | bug fixes |
  | `refactor/` | restructuring with no behaviour change |
  | `docs/` | documentation only |
  | `test/` | tests only |
  | `build/`, `ci/` | CMake, dependencies, GitHub Actions |
  | `chore/` | repository housekeeping |

  Milestone work includes the milestone number: `feat/m2-rmlui`,
  `feat/m3-quartic-solver`. Large milestones are split into several topic
  branches that each merge on their own, rather than one long-lived branch.
- Use lowercase and hyphens only. No spaces, parentheses or other characters
  that need quoting in a shell.

## Commits

[Conventional Commits](https://www.conventionalcommits.org/):
`type(scope): summary in the imperative, lowercase, no full stop`.

- **Types:** `feat`, `fix`, `refactor`, `perf`, `test`, `docs`, `build`, `ci`, `chore`.
- **Scopes:** `app`, `input`, `ui`, `render`, `camera`, `sim`, `physics`, `rules`,
  `data`, `audio`, `ai`, `ml`, `ecs`, `core`, `platform`.
- The body says why, and lists behaviour changes a player would notice.
- One logical change per commit; the build and tests pass at every commit.
- No generated files, build output, `graphify-out/`, credentials or personal paths.

## Merging

1. Rebase the topic branch on the latest `main` and make sure it builds
   warning-free and `ctest` passes.
2. Open a pull request using the template; CI must be green on macOS, Linux
   and Windows.
3. Merge with a merge commit (`git merge --no-ff`), titled after the work,
   e.g. `Merge M2: RmlUi integration`. This keeps each feature's commits grouped
   in `git log --graph` and lets a whole feature be reverted in one step.
4. Delete the topic branch after merging.

Never rewrite `main` (no force-push, no rebasing published history).

## Releases and versions

- [Semantic Versioning](https://semver.org/). Before 1.0, each completed
  milestone bumps the minor version: M1 → `0.2.0`, M2 → `0.3.0`, and so on.
  Fixes between milestones bump the patch version.
- The version lives in `project(... VERSION x.y.z)` in `CMakeLists.txt`.
- A release is an annotated tag on `main` (`git tag -a v0.3.0 -m "M2: UX foundation"`)
  made in the same step as moving the *Unreleased* section of `CHANGELOG.md`
  under the new version.

## Changelog

`CHANGELOG.md` follows [Keep a Changelog](https://keepachangelog.com/). Every
pull request that changes what a player sees, or something a contributor
needs to know, adds a line under *Unreleased* (Added, Changed, Fixed, Removed).

## Binary assets

Text is stored with LF line endings and binary formats are marked in
`.gitattributes`. When large assets arrive (models, HDR environments, audio,
ONNX models, from milestone M6 on), they move to Git LFS. Enabling LFS
requires merging its hooks with the existing graphify hooks in `.git/hooks/`.
ML training checkpoints never go in the repository; they live on the Hugging
Face Hub, and only shipped `.onnx` models are committed.
