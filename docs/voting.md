# Voting

Players choose the next arena and weapon preset during the voting phase.
The countdown shows the time remaining, and the fixed selection summary
keeps the player's current choices visible while browsing.

## Browsing choices

Arena and Weapons scroll independently. Each column reuses eight visible
option controls; eight is the visible pool size, not a limit on configured
choices. The range indicator shows which options are currently in view.

- Move the mouse wheel over a column to scroll it by three options.
- Use that column's arrows to move by eight options.
- Click its scroll track to jump to another position in the list.
- Select an option to vote for it. Scrolling does not change the selection.

When `AllowRandomChoice` is enabled, columns with at least two options keep
their Random choice pinned outside the scrolling list. The selection summary stays fixed, so a
choice remains visible even when its ordinary option has scrolled away.

`ArenaSelection` and `PresetSelection` still control whether each category is
voted on or chosen randomly by the server. Setting either to `"random"`
shows a server-selected status in that column instead of choices; the server
rolls its choice each round.

## Appearance

Voting uses the leaderboard's dark panels, accent colors, logo, and community
branding. Shared `themes.json` values apply to both screens; optional sparse
`vote-ui-theme.json` values customize voting independently. See
[UI themes](themes.md) for configuration and inheritance, and
[community branding](branding.md) for custom logo installation.

## Option delivery

The server sends each column's labels in array chunks of at most 100 entries.
Labels are limited to 128 characters and line breaks/tabs are flattened;
the original option indices are preserved for voting. Clients publish each
complete received list atomically, rather than showing partially received
choices. Late joiners receive the cached option lists for the current vote.

The fixed widget pools bound the number of visible controls even with longer
lists. See [validation notes](vote-ui-validation.md) for the scenarios tested;
the pool and transport bounds alone are not a large-list runtime benchmark.
