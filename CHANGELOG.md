# Changelog

Maintenance rule: work accumulates under [Unreleased]; a release moves it to
a [X.Y.Z] heading in the same commit that bumps `DmVersion.VERSION` and
`config.cpp` (CI enforces the two stay in lockstep). Tag after the boot smoke
passes, not before.

## [Unreleased]

### Added
- Phase 0 scaffold: config.cpp, version anchor, JSON config loader with
  clamp floors, RPC id block, round-phase model, round engine skeleton
  (single 500 ms timer, population gate), public observer API (DmApi),
  consolidated PlayerBase and MissionServer hook blocks, boot fixtures.
- Build script (Mikero MakePbo + optional DayZ Tools signing).
- CI: static gates (ASCII, continuation style, version lockstep, single
  chain link, no OnUpdate, fixture coverage, description budget) and secret
  scanning.
- Contributor guide with testing standards.
