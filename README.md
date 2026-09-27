# Sentinel Deathmatch

A high-performance, fully configurable deathmatch mod for DayZ Standalone.

**Status: playable alpha (pre-release).** The round loop, voting, respawn,
arena loading, HUD, and in-game scoreboards are usable for local playtesting.
Expect unfinished presentation, compatibility gaps, and changes to both the
UI and configuration before a stable release.

## What it will do

- **Round-based deathmatch** with a real lifecycle: vote, countdown, live
  round, scoreboard - not a static free-for-all.
- **In-game voting UI**: players vote each round on the weapon preset and the
  arena, and each column offers a "Random" pick (`AllowRandomChoice`). Want
  one side always random? Set `"PresetSelection": "random"` (or
  `"ArenaSelection": "random"`) in `config.json` and that column disappears
  from the vote; the server rolls it each round.
- **Operator-authored arenas**: an arena is a boundary + spawn points + an
  object set. Build yours in DayZ Editor, save, drop the `.dze` file in your
  profile folder, list it in `zones.json`. Players vote between your arenas
  round to round.
- **Weapon presets** in plain JSON: primary/secondary with attachments and
  mags, clothing, gear. Validated at boot - a typo disables the preset with a
  loud log line instead of breaking a round.
- **Fast respawn**: no respawn dialog, configurable delay, spawn protection,
  smart spawn selection away from enemies.
- **Zone confinement**: soft-wall damage or hard teleport, with on-screen
  boundary warnings. Optional shrinking-zone mode.
- **In-game leaderboard**: live round standings and session totals, with a
  compact scrollable player list and a pinned local-player row.
- **Community branding**: operators can replace the community name, subtitle,
  colors, promotional footer, and logo on the leaderboard and voting screen. The required
  `Powered by Sentinel Deathmatch` credit remains readable in the UI.
- **Built for density**: engineered for lots of players shooting in one small
  area - no per-frame script work, event-driven networking, rate-limited
  cleanup, and a mission package that strips the map to the minimum.

## Performance posture

This project treats server FPS as the primary feature. The engineering rules
(no per-frame work, single dispatch-chain links, bail-before-allocate,
measured cost bounds for hot-path changes) are documented in
[CONTRIBUTING.md](CONTRIBUTING.md) and enforced in review and CI.

## Run a server

A cookie-cutter quickstart - SteamCMD to running deathmatch server in under
an hour - lives in [server-example/](server-example/README.md), including an
example `serverDZ.cfg` and the arena mission overlay.

### Custom arenas from DayZ Editor

Build an arena in DayZ Editor, save it, drop the JSON `.dze` file in
`$profile:SentinelDeathmatch\arenas\`, and reference it from the zone in
`zones.json` (`"DzeFile": "my_arena.dze"`). The mod reads the file directly -
no converter and no loader mod required. When that arena wins a vote its
objects materialize before anyone is teleported in; when it rotates out they
are removed a full round later, after everyone has long left the area.

**The whole arena can live in the Editor** via `dm:` markers: rename any
placed object (its DisplayName) to a directive and it becomes geometry
instead of scenery - it never spawns in-game:

| Marker | Meaning |
|---|---|
| `dm:center` | the combat zone's center |
| `dm:edge` | sizes the circle - farthest edge marker from center wins; drag it out for a bigger arena |
| `dm:spawn` | a spawn point; the marker's rotation is the spawn facing, its exact height is kept (rooftops work) |
| `dm:lobby` | the pre-round lobby position |

Any geometry the markers provide overrides the zone's numbers in
`zones.json`; anything missing falls back to them. That means an "arena"
over an existing map location - a military compound, a village - needs no
placed scenery at all: markers only, ten minutes in the Editor.

Rules of the road: classname objects only for now (`.p3d` path placements
are skipped with a log); every classname is validated against the server's
config at boot, and an arena referencing a mod the server doesn't run is
disabled loudly rather than failing mid-round; keep object counts sane
(warning above 300, hard refusal above the configurable `MaxArenaObjects`);
derived radii clamp to 50-300 m (the single-network-bubble rule). Binary
`.dze` saves aren't readable - re-save as JSON in the Editor. See
[docs/examples/arenas/](docs/examples/arenas/) for the file shape.

### Modded weapons

Presets are not limited to vanilla. Any classname from any mod your server
loads works in `presets.json` - .50 cals, 20mm anti-materiel rifles, .408s,
whatever your mod stack provides. Put the weapon's classname in
`PrimaryClass`, its magazine in `PrimaryMagClass` (or the ammo classname for
internal-magazine rifles), and its attachments in `PrimaryAttachments`,
exactly as you would for vanilla gear.

Every classname is validated against the server's config tree at boot: an
entry the server can't resolve disables that preset with a log line instead
of breaking rounds, so a preset file shared between servers with different
mod stacks degrades gracefully.

Long-range presets deserve a matching arena - crank the zone `Radius`
toward the single-bubble limit and make sure `networkRangeFar` covers the
zone diameter (see the zones example), or snipers will be shooting at
players the server never told their client about.

## Building

Windows + [Mikero DePboTools](https://mikero.bytex.digital/):

```
.\tools\build.ps1
```

Output: `build/@SentinelDeathmatch/`. See [CONTRIBUTING.md](CONTRIBUTING.md)
for the full local loop and testing standards.

## Leaderboard, voting, and community branding

Press **P** to toggle the leaderboard. It also opens automatically at round end. It offers round and server-session views, keeps the local
player visible, and includes a **Find Me** control for long player lists.

The voting screen uses the same dark panel, accent colors, and community
branding as the leaderboard, with clear arena and weapon choices, selected
votes, and a countdown. Each column scrolls independently through its full
option list while Random and the selection summary remain pinned. See the
[voting guide](docs/voting.md) for controls and option delivery.

Server operators can theme both screens without changing the Sentinel
Deathmatch PBO. `themes.json` supplies shared defaults; optional
`leaderboard-theme.json` and `vote-ui-theme.json` files override individual
fields per screen. The server reads these files from
`$profile:SentinelDeathmatch\` at boot and sends the validated themes to
clients. Existing leaderboard configurations keep working. See
[UI themes](docs/themes.md), [the leaderboard guide](docs/leaderboard.md), and
[the branding guide](docs/branding.md), including a companion-addon example
for a custom `.paa` logo.

## License

Source-available under a custom, non-OSI license: free to run on any server,
**including monetized servers**, with attribution retained. The branding
exception permits original companion branding add-ons without repacking or
redistributing Sentinel Deathmatch. All other rights remain reserved; see
[LICENSE.md](LICENSE.md).

## Credits

This mod interoperates with, and its design was informed by, the work of
others in the DayZ modding community - see [docs/CREDITS.md](docs/CREDITS.md).
No third-party code is included in this repository.
