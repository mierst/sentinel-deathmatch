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

- Final production-layout visual and mouse-control review at 720p, 1080p,
  and 1440p, including the configurable logo and independent attribution.
- Round-end auto-open and visual behavior across a population reset.
- Public CI, release approval, Workshop distribution, and live-server
  validation.

The native prototype was visually tested separately. That does not replace
the final production-layout check: screenshot capture was unavailable during
the integration run, so this document does not claim visual approval.
