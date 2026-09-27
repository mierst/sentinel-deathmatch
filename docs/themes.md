# Leaderboard and voting themes

Both menus use a shared visual style and server-owned branding. Theme files
live beside `config.json` in `$profile:SentinelDeathmatch\` (the directory
under the server's selected profile). Clients receive the accepted themes;
players do not need their own JSON files. Restart the server after editing.

## Shared defaults and screen overrides

Each screen resolves its theme in this order, with later layers taking
precedence for fields they explicitly contain:

1. Built-in neutral defaults.
2. `themes.json`: shared branding and colors for both screens.
3. `leaderboard-theme.json` for the leaderboard, or `vote-ui-theme.json` for voting.

Override files are optional and may contain just one field. Omitted fields
inherit. Explicit `false`, `0`, or `""` replace the inherited value; they do
not mean "use the default." For example, `"ShowLogo": false` hides the logo,
`"Footer": ""` clears inherited promotional text, and `"AccentR": 0` sets the
red component to zero. The separate required `Powered by Sentinel Deathmatch`
credit cannot be changed through these files.

Start by copying [themes.json](examples/themes.json) into the profile directory.
To change only the voting subtitle and footer, also copy
[vote-ui-theme.json](examples/vote-ui-theme.json). The remaining voting fields
inherit from the shared file. Leave out both screen overrides to use one
appearance everywhere.

Existing `leaderboard-theme.json` files continue working without migration.
Files are not automatically rewritten. The
[full leaderboard example](examples/leaderboard-theme.json) explicitly sets
every supported field and therefore overrides every shared value for that
screen. When adopting shared defaults, remove only the fields you want to
inherit from an existing screen override.

## File selection and validation

These optional, append-only settings in `config.json` choose the files:

```json
"ThemesFile": "themes.json",
"LeaderboardThemeFile": "leaderboard-theme.json",
"VoteUIThemeFile": "vote-ui-theme.json"
```

Each value is a 6-80 character `.json` basename. The stem accepts only
letters, digits, underscores, and hyphens; paths, directory separators, and
`..` are rejected. Invalid filename settings use the corresponding default
filename above. See the [complete config example](examples/config.json).

A missing file contributes no overrides. A malformed layer is ignored as a
whole, so it cannot partially change the inherited theme. Valid numeric
values are clamped to their supported ranges; text control characters are
flattened to spaces. See the [field reference](leaderboard.md#theme-file)
for all fields, defaults, length limits, and ranges; both screens support
the same fields.

## Logos and attribution

`LogoPath` must reference a packaged local texture, never a URL. Load the
texture-providing companion add-on on the server and every client. Setting
it in `themes.json` shares the logo across both screens; either override can
select a different logo or hide it with `ShowLogo`.

See [community branding](branding.md) for texture conversion, packaging,
signing, installation, and the companion-branding license exception.
