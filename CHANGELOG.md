# Changelog

Maintenance rule: work accumulates under [Unreleased]; a release moves it to
a [X.Y.Z] heading in the same commit that bumps `DmVersion.VERSION` and
`config.cpp` (CI enforces the two stay in lockstep). Tag after the boot smoke
passes, not before.

## [Unreleased]

### Changed
- Sentinel platform integration collapsed from the separate dm_sentinel
  bridge PBO into the core mod behind `#ifdef SentinelEnforcer` (loaded mods
  are compile defines). One Workshop item instead of two, and no
  requiredAddons that could fire the unavoidable Windows "requires addon"
  dialog on a misconfigured server. The feed remains optional at three
  levels: enforcer master enable, sentinel.json (EventFeed,
  GlobalLeaderboard), and per-event capture flags. Emitters compile out
  entirely on clients and enforcer-less servers.

### Added
- Phase 1 round services: full FSM loop (IDLE -> VOTING -> COUNTDOWN -> LIVE
  -> ROUNDEND -> VOTING, population gate back to IDLE in any phase);
  zones.json (boot-validated, demo zone written on first boot) with soft-wall
  damage / teleport enforcement on the engine tick; presets.json
  (boot-validated against the config tree, vanilla-only defaults) with
  frame-staged loadout injection; server-driven respawn (CreatePlayer +
  SelectPlayer, no dialog) with smart farthest-from-enemies spawn selection
  and fail-safed spawn protection; vote tally with rate-limited VOTE_CAST
  RPC handling, random tie-break; per-round scoring with streaks, score-limit
  round end, log scoreboard; rate-limited corpse cleanup service; join path
  spawns straight into the loop with the active loadout.
- Phase 0 scaffold: config.cpp, version anchor, JSON config loader with
  clamp floors, RPC id block, round-phase model, round engine skeleton
  (single 500 ms timer, population gate), public observer API (DmApi),
  consolidated PlayerBase and MissionServer hook blocks, boot fixtures.
- Build script (Mikero MakePbo + optional DayZ Tools signing).
- CI: static gates (ASCII, continuation style, version lockstep, single
  chain link, no OnUpdate, fixture coverage, description budget) and secret
  scanning.
- Contributor guide with testing standards.
