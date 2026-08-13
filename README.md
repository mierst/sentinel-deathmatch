# Sentinel Deathmatch

A high-performance, fully configurable deathmatch mod for DayZ Standalone.

**Status: early development (pre-release).** The architecture is settled and
the scaffold boots; gameplay systems are landing phase by phase. Nothing here
is playable yet.

## What it will do

- **Round-based deathmatch** with a real lifecycle: vote, countdown, live
  round, scoreboard - not a static free-for-all.
- **In-game voting UI**: players vote each round on the weapon preset and the
  arena.
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
- **Server-owner branding**: MOTD, colors, logo slots, per-preset icons.
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

## License

Source-available under a custom license: free to run on any server,
**including monetized servers**, with attribution retained. Final license
text is being prepared; until it is published in this repository, all rights
are reserved. See [LICENSE.md](LICENSE.md).

## Credits

This mod interoperates with, and its design was informed by, the work of
others in the DayZ modding community - see [docs/CREDITS.md](docs/CREDITS.md).
No third-party code is included in this repository.
