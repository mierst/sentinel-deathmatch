# Leaderboard

The leaderboard is part of the playable alpha. Press **P** to toggle it; it also opens automatically at round end. During a live round
it shows fresh local standings; the two view controls switch between the
current round and the totals since this server process started.

The list reuses a fixed pool of 18 row widgets. Its visible-row count depends
on display resolution (the current layouts show about 8 rows at 720p and 14
at 1080p), so do not depend on either number as a presentation contract. It
scrolls for larger servers, keeps the local player's row pinned for context,
and provides **Find Me** to bring that player's ordinary list row into view.
The panel says when there are no standings yet instead of drawing blank
columns.

Use the mouse wheel to move three rows at a time. The page arrows move one
viewport at a time, and the scroll track jumps to its selected position.
Round and session data are fetched only for the active view in pages of up to
100 rows. While open, the active view refreshes once per second; closed
menus do not poll. Rankings are cached until scores change.

## Theme file

The server owns the leaderboard theme. Add this append-only setting to
`$profile:SentinelDeathmatch\config.json`:

```json
"LeaderboardThemeFile": "leaderboard-theme.json"
```

The value must be a 6-80 character `.json` basename. Before the `.json`
suffix, only letters, digits, underscores, and hyphens are accepted; no
directory separators, `..`, or paths. An invalid or missing value falls back
to `leaderboard-theme.json`.
At boot, the server loads the selected file from
`$profile:SentinelDeathmatch\`, validates it, falls back to neutral defaults
where necessary, and sends the accepted theme to clients through the game
RPC path. Change the file and restart the server; themes do not hot-reload.

Start with [leaderboard-theme.json](examples/leaderboard-theme.json). Omitted
fields keep their neutral defaults. Numeric values outside their allowed range
are clamped to the nearest bound, while line-control characters in text are
flattened to spaces. A malformed JSON document falls back as a whole to the
neutral theme.

| Field | Default | Rules |
|---|---|---|
| `BrandName` | `COMMUNITY` | Text, maximum 32 characters. |
| `Subtitle` | `DEATHMATCH` | Text, maximum 48 characters. |
| `Footer` | empty | Text, maximum 128 characters. |
| `LogoPath` | empty | Packaged local texture path, maximum 160 characters; URLs are refused. |
| `ShowLogo` | `true` | Boolean. Hides only the configurable logo. |
| `AccentR`, `AccentG`, `AccentB` | `59`, `130`, `246` | Integer RGB components from 0 through 255. |
| `SurfaceR`, `SurfaceG`, `SurfaceB` | `7`, `8`, `10` | Integer RGB components from 0 through 255. |
| `PanelOpacity` | `0.97` | Number from 0 through 1. |

Long text is measured in the UI and ellipsized to fit. `LogoPath` is a local
PBO path such as `CommunityBrand\data\community_logo.paa`; it is never a
web address. A non-empty logo path requires the companion add-on containing
that texture to be loaded by both the server and every client.

The bottom credit, `Powered by Sentinel Deathmatch`, is deliberately separate
from `Footer`. It remains visible and readable under every theme, including
one with an empty footer or `ShowLogo` disabled.

For an end-to-end custom-logo example, see
[the branding guide](branding.md).
