# Mission overlay

Files here are copied OVER a vanilla mission copy (see the parent README,
step 3). What they do, plus two small manual edits the overlay cannot do
for you because those vanilla files vary by map and game version.

## What the overlay replaces

- `db/economy.xml` - master switches: loot economy, infected, animals,
  vehicles, dynamic events all OFF; player persistence stays ON.
- `db/events.xml` - emptied (no dynamic events).

## Manual edit 1: db/globals.xml (in your copied mission)

Change these values in place - do not replace the whole file, the rest of
it is map/version specific:

| Variable | Set to | Why |
|---|---|---|
| `ZombieMaxCount` | `0` | no infected |
| `AnimalMaxCount` | `0` | no animals |
| `CleanupLifetimeDeadPlayer` | `60` | corpse gear-grab window backstop (the mod's own cleanup is primary) |
| `CleanupLifetimeDefault` | `45` | dropped-item backstop |
| `TimeLogin` | `5` | fast join |
| `TimeLogout` | `5` | fast leave |
| `TimePenalty` | `20` | combat-log timer |

## Manual edit 2: cfggameplay.json (in your copied mission)

Set:

```
"disableRespawnDialog": true
```

(under `GeneralData`). The mod respawns players server-side with no dialog;
this stops the vanilla respawn screen from flashing up on death.

Also confirm `serverDZ.cfg` has `enableCfgGameplayFile = 1;` if your
version requires it for cfggameplay.json to be read.
