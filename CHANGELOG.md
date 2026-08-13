# Changelog

Maintenance rule: work accumulates under [Unreleased]; a release moves it to
a [X.Y.Z] heading in the same commit that bumps `DmVersion.VERSION` and
`config.cpp` (CI enforces the two stay in lockstep). Tag after the boot smoke
passes, not before.

## [Unreleased]

## [0.1.2] - 2026-08-12

Combat-readiness release: spawn, aim, fire.

### Added
- Every spawn now includes one random melee weapon from a configurable
  `MeleePool` in presets.json (vanilla default pool: knives, machete,
  hatchet, bat, crowbar, pipe wrench). Disable via `MeleeSpawn: 0` in
  config.json; an empty/missing pool reseeds the defaults (the engine's
  JSON loader clears arrays absent from older files, so empty cannot
  mean "off").
- Fixed hotbar layout: slot 1 gun, slots 2-3 bandages, slot 4 melee,
  slot 5 sidearm (presets carrying both a primary and a secondary).

### Changed
- Loadout weapons now spawn ready to fire: full magazine attached (or
  internal magazine filled) and a round chambered, via the vanilla
  `SpawnAmmo` cascade. No more racking the bolt on spawn.
- `PrimaryMagClass`/`SecondaryMagClass` now also accept an ammo classname
  for internal-magazine weapons (e.g. `"Ammo_12gaPellets"` on a shotgun
  preset) so the gun loads with exactly that type; left empty, the engine
  picks a chamberable type at random.
- Default "Shotgun CQB" preset loads `Ammo_12gaPellets` explicitly.

## [0.1.1] - 2026-08-12

First live-server feedback release.

### Added
- `DisableSurvivalPressure` config (default on): water, energy, heat
  comfort, and stamina are pinned every ~10 s - no thirst, hunger, cold, or
  exhaustion in the arena.
- Loadout weapons bind to the hotbar: the main weapon (primary, or the
  secondary in pistol-only presets) lands in hands AND hotbar slot 1;
  a sidearm alongside a primary binds to slot 2.
- If spawning into hands is refused mid-transition, the weapon falls back
  to the inventory instead of being silently lost.

### Fixed
- meta.cpp in the shipped mod folder now carries the item's own publishedid
  (3782135939) - required for DZSA and launcher mod resolution on servers
  running the mod.

## [0.1.0] - 2026-08-12

First alpha playtest release. Steam Workshop (hidden during initial
testing): https://steamcommunity.com/sharedfiles/filedetails/?id=3782135939

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
