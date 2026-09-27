# Voting UI validation

Local development validation, 2026-09-27. This records the v0.1.31 voting
UI and layered themes; it is not a Workshop or live-server release record.

## Scrolling and large-list validation

The final scrolling build boots with **61 passing fixtures** and no server
or client script errors. Four additional fixtures cover 1,000-option assembly,
out-of-order and duplicate chunks, stale/new windows, malformed bounds, final
partial chunks, and display-label sanitization. Static gates pass.

The actual native client received 100 arenas and 120 weapon presets at 720p.
Scripted wheel events moved each column independently by three entries; the
server received indices 3/3. Bottom-track clicks followed by the eighth visible
button sent indices 99/119. Returning Arena to the top preserved weapon 119;
the pinned weapon Random button sent 120.

The final 1080p run used 1,000 configured arenas and 1,000 weapon presets,
including names longer than the 128-character wire limit and a line break in
the last arena name. Both complete lists arrived with exactly 1,000 entries.
Page-arrow clicks sent indices 8/8, bottom-track clicks reached 999/999,
scrolling beyond the bottom stayed at 999, Random sent 1,000, and returning
to the top sent 0/0. These indices were observed at the server RPC receiver.
Hover state restored correctly and long visible labels fit their widgets.
The client uses fixed pools of eight option buttons per column, independent
of the number of configured choices.

These checks invoke native widget handlers through a local development test driver;
they do not substitute for physical mouse-wheel inspection. The owner
accepted the initial 1080p appearance before scrolling was added. The owner also accepted the final scrolling appearance. Live resize, same-process reconnect, and late-join
runtime checks remain unverified. The connection reset and cached late-join
path were reviewed in code. No live server or Workshop changes were made.

The sections below retain the initial styling validation for reference.

## Build and engine checks

- Static gates pass; all JSON examples parse; the generated vote layout is
  reproducible with `node tools/generate-vote-layout.cjs`.
- Signed PBO built successfully. A local dedicated server boots with all
  **57 fixtures passing**, no script compile errors, and no script errors.
- The client compiles and joins with the same PBO and a local development test driver.
  The driver is not part of the mod distribution.
- Four normal boot fixtures cover sparse inheritance, explicit false/zero/
  empty values, independent screen copies, and filename validation.
- Two additional negative JSON probes passed during development. They live
  in `DmLeaderboardThemeStore.SelfTestInvalidJson()` for explicit test-mission
  invocation, because the engine logs intentional malformed input as errors.
  They are not invoked during ordinary community-server startup.

## Native client measurements

The test driver opens the actual `DmVoteMenu` and queries native widgets.
These are runtime geometry checks, not screenshot or human visual approval.

| Client viewport | Panel origin | Panel size | Fully inside viewport |
|---|---|---|---|
| 1280 x 720 | 215, 30 | 850 x 660 | Yes |
| 1920 x 1080 | 460, 130 | 1000 x 820 | Yes |
| 2560 x 1440 | 630, 187 | 1300 x 1066 | Yes |

Long option names were injected into all eight slots at 720p and 1440p;
the final labels measured within their allotted widths. The independent
attribution measured within its backing at all three resolutions. A shared
brand and accent arrived over the real server RPC, alongside distinct
leaderboard/voting subtitles. The voting override correctly cleared the
footer, hid the logo, and set the red component to zero while retaining the
shared green component.

At 1080p, scripted clicks on real arena/weapon choices and Random updated
the selected indicators and summary. Switching back from Random selected
the ordinary choice again. Native hover handlers changed the fill and
restored its previous selected color on leave. No client script errors or
VM exceptions were observed in these final checks.

Physical mouse/keyboard interaction, final visual appearance, and custom
logo rendering still require human inspection. Live resizing and late theme
delivery preserve selections by implementation, but were not exercised as
separate runtime scenarios in this validation.

## Runtime cost

Theme files are read once at server startup. Each joining player receives
one additional small resolved-theme RPC. The existing open-menu HUD timer
updates the countdown and checks theme/resolution changes; there is no new
poller or per-frame callback. Layout/text fitting happens on open, option,
selection, theme, or resolution changes. Pointer entry/exit repaints at most
two button fills without rebuilding text. That tested revision exposed only
eight options per column plus Random. The scrolling revision retains eight
pooled visible controls per column while removing that total-option display
limit; its scrolling and transport measurements will be recorded separately.
