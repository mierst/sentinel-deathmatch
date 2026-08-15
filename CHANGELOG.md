# Changelog

Maintenance rule: work accumulates under [Unreleased]; a release moves it to
a [X.Y.Z] heading in the same commit that bumps `DmVersion.VERSION` and
`config.cpp` (CI enforces the two stay in lockstep). Tag after the boot smoke
passes, not before.

## [Unreleased]

## [0.1.18] - 2026-08-15

### Fixed
- Preset attachments that mount onto another attachment (a scope onto a
  rail base - common in modded weapon packs) now land correctly: direct
  slot attach first, nested-inventory placement as fallback. List order
  matters: base before scope.

## [0.1.17] - 2026-08-15

### Added
- Meta-presets via `RandomFrom` in presets.json: a preset naming other
  presets becomes a per-spawn random pick among them - "Free For All"
  gives every player in the lobby a different loadout every life.
  Members must be ordinary presets; unknown names are skipped and an
  empty membership disables the meta-preset loudly.

### Fixed
- Last round's dropped guns no longer litter the arena: a round-start
  sweep queues every loose ground item inside the chosen zone through
  the rate-limited cleanup pipe. Items held by players, items inside
  containers, and arena furniture are untouched.

## [0.1.16] - 2026-08-15

### Fixed
- "Login timed out (WaitPreloadCamRespawnState)" kicks on respawn: the
  mod's server-timer respawn raced the client's own auto-respawn login
  (the respawn dialog is disabled, so the client initiates one itself),
  sometimes creating duplicate bodies and stranding the client's login
  state machine. Respawn now rides the engine's own respawn login
  end-to-end - the mod only remembers the death position (60 s memory)
  so the new spawn still avoids the death spot, and spawn protection now
  applies on this path too. `RespawnDelaySeconds` is deprecated (respawn
  timing is the client's death-screen flow now).

## [0.1.15] - 2026-08-15

### Fixed
- Respawning no longer strands the mouse cursor on screen (the vanilla
  death-screen focus state lingered because server-driven respawn never
  clicks the respawn button). The HUD now detects the body swap and
  restores game focus automatically - no more ESC-in, ESC-out dance,
  and the cursor can't wander onto a second monitor mid-fight.

## [0.1.14] - 2026-08-14

### Added
- `dm:` markers make the ENTIRE arena Editor-authored: rename any placed
  object to `dm:center`, `dm:edge` (farthest sizes the circle),
  `dm:spawn` (rotation = facing, height kept), or `dm:lobby` and it
  becomes zone geometry instead of scenery. Marker geometry overrides
  zones.json; derived radii clamp to 50-300 m. An arena over an existing
  map location needs markers only - no placed objects at all.

## [0.1.13] - 2026-08-14

### Added
- Respawns avoid the death spot: spawn points within
  `RespawnAvoidDeathMeters` (default 75, 0 disables) of where the player
  died are filtered out before the away-from-enemies pick, with a
  fallback to all points on tiny arenas.

## [0.1.12] - 2026-08-14

### Added
- DayZ Editor arena import: a zone's `DzeFile` references a JSON `.dze` in
  `$profile:SentinelDeathmatch\arenas\` and the mod spawns it - no
  converter, no loader mod. Objects materialize when the arena wins a vote
  (before anyone teleports in) and retire a full round after it rotates
  out; same-arena repeats touch nothing. Classnames validate at boot (a
  bad arena disables its zone loudly), object budgets are configurable
  (`MaxArenaObjects`, spawn/delete per-tick rates), EditorOnly markers are
  skipped, and `.p3d` path placements are deferred to a later release.

## [0.1.11] - 2026-08-14

### Added
- The leaderboard shows every connected player from the moment they join,
  not just those who already have a kill or death.
- Join/leave announcements in the killfeed and chat, deduplicated so
  respawns never spam the feed.

## [0.1.10] - 2026-08-14

### Added
- Full heal at round start: blood, bleeding sources, shock, and broken
  legs all reset when the countdown teleports everyone in - last round's
  wounds stay in last round.

## [0.1.9] - 2026-08-14

### Fixed
- Client freeze when approaching the zone edge: the marker system spawned
  its whole pillar window (up to 7 heavy smoke emitters) in one frame, and
  the first spawn also paid the particle asset load. Pillars now light one
  per tick, and the asset load happens hidden at mission start.
- Vote menu could fail to appear for the whole vote window: any open menu
  (the chat box included - fatal if you were mid-/mapvote at the
  transition) vetoed the one-shot auto-open, stranding players in a vote
  they couldn't see. The open now retries every tick until it lands, and
  a standing HUD hint ("Voting open - press B") covers the gap.
- /mapvote outside a live round now answers in chat instead of being
  silently ignored - typing it during a vote window used to just vanish.

## [0.1.8] - 2026-08-14

### Added
- In-round leaderboard: the server pushes fresh standings after every
  scoring change, so opening the scoreboard mid-round shows live
  numbers ("Round standings") instead of last round's result. Auto-open
  still only happens at round end.

### Changed
- HUD killfeed lines each draw on their own semi-transparent backing
  strip, with breathing room between the text and the screen edge.

## [0.1.7] - 2026-08-14

### Added
- `DisableUnconsciousness` (config.json, default on): going unconscious
  kills outright - arena rounds have no room for a 3-minute nap.
- Last-attacker memory: deaths the engine can't attribute directly -
  bleed-outs, unconscious finishes, choosing respawn while unconscious -
  now credit whoever actually put the victim down. The kill-point
  penalty only applies to genuinely self-inflicted deaths; respawning
  while unconscious never costs a kill.
- `PlayerCosmetics` in presets.json: per-steam64 full clothing override,
  validated at boot like everything else. Weapons and gear unaffected.

### Fixed
- Scoreboard columns actually align now: rows render into four
  independent column widgets (names left, numbers right) instead of one
  space-padded text blob, which a proportional font could never line up.

## [0.1.6] - 2026-08-14

### Added
- The zone warning now shows the actual grace countdown
  ("RETURN TO THE ZONE - 7"): the server pushes its authoritative timer
  to the offending player once per displayed second - nobody else pays
  any traffic - and the HUD falls back to the plain warning if an update
  hasn't arrived within 2 s.

## [0.1.5] - 2026-08-14

Boundary release: see the edge, change the map.

### Added
- In-world zone edge markers: red smoke pillars along the boundary arc,
  visible whenever you're within 60 m of the edge (either side). Client-
  side only, a sliding window of at most 7 emitters near the player -
  never the whole ring - and positions snap to a fixed arc grid.
- `/mapvote` chat command: once two thirds of the lobby (rounded up) has
  typed it, the round ends on the spot and arena+preset voting reopens.
  Progress announces in the killfeed; each player counts once per round;
  the command is swallowed before it reaches chat.

## [0.1.4] - 2026-08-14

Live-playtest release: everything a solo tester couldn't see or survive.

### Added
- Killfeed lines are also delivered to every player's chat
  (`KillfeedToChat` in config.json, default on). The HUD rows expire after
  8 seconds - which the victim spends dead; chat scrollback survives the
  respawn blackout.
- New zone enforcement mode `"countdown"`: leave the arena and a grace
  timer (`OutOfZoneKillSeconds`, default 10, floor 3) starts; return to be
  forgiven, stay out and die instantly. The killfeed reports it as
  "left the zone".
- Score penalties: deaths with nobody to credit - suicides and zone
  enforcement - now cost the victim a kill point on top of the death.
  Negative scores are allowed.

### Fixed
- Vote menu and scoreboard now draw their semi-transparent backdrops. The
  layouts referenced a panel style the engine doesn't ship, so every
  background silently rendered as nothing and text floated over the world.

## [0.1.3] - 2026-08-12

Consensus release: when the room agrees, get to the shooting.

### Added
- Vote consensus fast-forward: once a strict majority of connected players
  votes for the same arena+preset combo, the remaining vote window clamps
  to `VoteConsensusSeconds` (config.json, default 10). Fires once per vote,
  only ever shortens, and the on-screen countdown updates immediately.
- README section on modded weapons in presets (any loaded mod's classnames
  work; boot validation degrades gracefully across differing mod stacks).

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
