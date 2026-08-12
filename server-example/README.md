# Stand up a Sentinel Deathmatch server

A cookie-cutter path from nothing to a running deathmatch server. Windows
commands shown; Linux differences noted inline. Budget 30-60 minutes.

## 1. Install the DayZ server

Via SteamCMD (app `223350`):

```
steamcmd +force_install_dir C:\dayz-server +login <steam_account> +app_update 223350 validate +quit
```

Linux: same app id; a Steam account that owns DayZ is required for Workshop
downloads later, so log in with a real account rather than `anonymous`.

## 2. Get the mods

Subscribe on Steam Workshop (or `+workshop_download_item 221100 <id>` in
SteamCMD, then copy from `steamapps/workshop/content/221100/<id>`):

| Mod | Workshop id | Load via | Needed by |
|---|---|---|---|
| Sentinel Deathmatch | `3782135939` | `-mod=` | server AND every client |
| Sentinel Enforcer (optional) | `3714349930` | `-serverMod=` | server only |

Copy each mod folder into the server root as `@SentinelDeathmatch` (and
optionally `@SentinelEnforcer`). Copy `@SentinelDeathmatch\keys\*.bikey`
into the server's `keys\` folder - clients are signature-checked against it.

The Enforcer is optional: without it the Sentinel platform integration
compiles itself out and the mod runs fully standalone. With it (plus a free
registration at the Sentinel platform), your server gets stats dashboards
and the deathmatch event feed.

## 3. Create the deathmatch mission

Start from the vanilla mission and overlay the deathmatch files:

```
robocopy C:\dayz-server\mpmissions\dayzOffline.chernarusplus C:\dayz-server\mpmissions\dayzOffline.deathmatch.chernarusplus /E
robocopy <this folder>\mission-overlay C:\dayz-server\mpmissions\dayzOffline.deathmatch.chernarusplus /E
```

(Linux: `cp -r` both.) The overlay switches the entire loot economy,
infected, animals, vehicles, and dynamic events OFF - see
`mission-overlay/README.md` for what it changes and the two small manual
edits it asks for (globals.xml numbers and one cfggameplay.json flag).

Any map works the same way - the mod is map-agnostic; your arenas live in
config, not the mission.

## 4. Server config

Copy `serverDZ.cfg` from this folder into the server root and edit the
hostname. Notable deathmatch-relevant settings are commented inline
(third-person lock, respawn time, network tuning for small arenas).

## 5. First boot

```
cd C:\dayz-server
DayZServer_x64.exe -config=serverDZ.cfg -port=2302 "-profiles=profile" "-mod=@SentinelDeathmatch" -dologs -adminlog
```

Add `"-serverMod=@SentinelEnforcer"` if you run the Enforcer. Linux binary:
`DayZServer`. The first boot writes default configs to
`profile\SentinelDeathmatch\`:

- `config.json` - round timings, score limit, respawn, min players
- `zones.json` - arenas: boundary circle + spawn points (a demo arena on
  Chernarus is written so the loop works out of the box)
- `presets.json` - two vanilla-weapon loadouts to start from
- `sentinel.json` - platform feed switches (only used with the Enforcer)

Watch `profile\script_*.log` for `[DM]` lines: boot fixtures, config
validation (`X of Y zones enabled`, `X of Y presets valid`), and
`round engine started`. A bad classname or zone disables that entry with a
log line - it never crashes the round loop.

## 6. Make it yours

Edit `zones.json` with arenas for your map (coordinates are world X/Z;
`Y: 0` snaps to terrain; 2+ spawn points per zone; keep each arena under
~600 m across so every player stays inside everyone's network bubble).
Edit `presets.json` with your weapon sets - every classname is validated at
boot against the mods you actually run. Restart between edits.

Reference copies of all config files live in `../docs/examples/`.

## 7. Go live

- Keep `verifySignatures = 2` (the example cfg default) so clients are
  checked against the shipped bikey.
- Port-forward/allow UDP 2302-2305 and the Steam query port 27016.
- Players need only the Sentinel Deathmatch Workshop item - the in-game
  browser and launchers resolve it automatically from the server's mod list.

## Troubleshooting

- **Stuck in IDLE**: fewer connected players than `MinPlayers` in
  config.json. For solo testing set it to 1.
- **Preset disabled at boot**: the log line names the classname the config
  tree could not resolve - typo, or the mod providing it is not loaded.
- **Clients kicked on connect**: missing bikey in `keys\`, or client mod
  list does not match the server's `-mod=` chain.
- **"requires addon" dialog**: you loaded a mod whose dependency is absent.
  Sentinel Deathmatch itself declares no dependencies and cannot cause this.
