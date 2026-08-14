# Arena `.dze` files

An arena file is plain JSON in the DayZ Editor save format. You can produce
one two ways: build visually in DayZ Editor and save, or write the JSON by
hand (or with tooling) - the mod cannot tell the difference.

## File shape

```json
{
    "MapName": "chernarusplus",
    "EditorObjects": [
        {
            "Type": "Land_Container_1Mo",
            "DisplayName": "anything, or a dm: directive",
            "Position": [4620.0, 339.5, 10010.0],
            "Orientation": [45.0, 0.0, 0.0],
            "Scale": 1.0,
            "EditorOnly": false,
            "Simulate": false
        }
    ]
}
```

Per object:

| Field | Meaning |
|---|---|
| `Type` | classname to spawn (validated against the server's config at boot; an unknown classname disables the whole arena's zone, loudly). `.p3d` paths are skipped for now. Ignored for `dm:` markers. |
| `DisplayName` | free text - unless it starts with `dm:`, which turns the entry into a geometry directive (below) |
| `Position` | `[X, Y, Z]` world meters. For `dm:spawn`, `Y = 0` means "snap to terrain"; any other Y is kept exactly (rooftop spawns work) |
| `Orientation` | `[yaw, pitch, roll]` degrees. For `dm:spawn`, yaw is the direction the player faces |
| `Scale` | 1.0 unless you mean it |
| `EditorOnly` | `true` = never spawned (Editor scratch objects) |
| `Simulate` | `false` (default) switches simulation off for dynamic props used as furniture; statics never simulate anyway |

## `dm:` geometry directives

Objects whose `DisplayName` is a directive are consumed as zone geometry and
never spawn. Anything they provide overrides the zone's numbers in
`zones.json`; anything absent falls back.

| DisplayName | Effect |
|---|---|
| `dm:center` | zone circle center = this position |
| `dm:edge` | zone radius = distance from center to this marker; place several, the farthest wins. Clamped to 50-300 m (single-network-bubble rule) |
| `dm:spawn` | a spawn point (yaw = facing; Y kept exactly, 0 = terrain snap). Any spawn markers REPLACE the zone's JSON spawn points; place at least 2 |
| `dm:lobby` | pre-round lobby position |

A marker's `Type` can be any object - it's just your visual handle while
editing and skips classname validation.

## Wiring it up

1. Drop the file in `$profile:SentinelDeathmatch\arenas\` (plain filename,
   no subdirectories).
2. Reference it from `zones.json`:
   `{ "Name": "VMC", "Enabled": 1, "Enforcement": "countdown", "DzeFile": "vmc.dze" }`
3. Restart the server. The boot log prints the derived geometry
   (`[DM] arenas: zone 'VMC' geometry from markers: ...`) and any problems.

An arena over an existing map location (a military compound, a village)
needs only markers - no scenery objects at all. See `vmc.dze` here for a
complete markers-only example and `example_arena.dze` for markers + scenery.
