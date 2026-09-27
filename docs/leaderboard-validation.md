# Leaderboard validation (0.1.30 candidate)

The acceptance target is **1,000 standings entries**, not 1,000 simultaneous
connected players. Measurements below use synthetic scores and one real
DayZ client on a local dedicated server, on 2026-09-27 with DayZ 1.29.

## Verified

- Dedicated-server and client script compilation succeeded. The optional
  integration compilation variant was also exercised.
- All 53 Sentinel Deathmatch boot fixtures passed.
- The real client received the server-selected custom theme.
- With 999 standings entries, the client received 100 rows at offset 0,
  99 rows at offset 900 (ranks 901-999), and a separate local-player row.
- Find Me returned the page containing the local player (rank 657, page
  offset 600). The session request returned its separate standings.
- The final row-array protocol produced no client script exceptions during
  these checks. Each response contains at most 100 separately encoded rows;
  the client rejects overlong rows and oversized row lists.
- Static repository gates, example JSON parsing, and whitespace checks pass.
- Manual inspection of the production UI at 1080p with 999 entries confirmed
  that standings load, scrolling works, and Find Me works. The inspector
  reported a brief initial loading state.
- After closing the menu, a local test round ending at the score limit
  caused leaderboard polling to resume; polling stopped as the next voting
  phase began. The server then entered the next live round.
- After the round population reset to one, requesting offset 900 returned
  the single player at offset 0 without an error.

## Measured cost

| Standings entries | First snapshot rebuild | Cached page + row encoding |
|---:|---:|---:|
| 60 | Below the clock's approximately 1 ms resolution | 0.10 ms |
| 240 | 4 ms | 0.09 ms |
| 1,000 | 19 ms | 0.16 ms |

The cached-page figure is the average of 100 requests in the local engine.
These are local measurements, not a server-FPS guarantee. A score change
invalidates the appropriate snapshot; the next request rebuilds it. Further
requests reuse that snapshot. Sorting preserves the previous tie order.

Previously, a kill rebuilt and broadcast both complete standings tables to
all clients. Now kills invalidate cached rankings; only clients with an open
leaderboard request pages. The existing 500 ms HUD timer drives the active
view's one-second polling. There is no new per-frame callback.

## Remaining release checks

- Additional production-layout visual checks at 720p and 1440p, and explicit
  custom-logo/attribution appearance checks, remain unverified.
- Release approval, Workshop distribution, and live-server validation.

The native prototype was visually tested separately. Production inspection
at 1080p was performed manually because automated screenshot capture was
unavailable; it does not establish appearance at the other resolutions.
