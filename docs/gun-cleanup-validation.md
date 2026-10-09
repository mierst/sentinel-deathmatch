# Gun cleanup validation

Local development validation, 2026-10-09. Release candidate v0.1.34 includes
main v0.1.33 and the gun cleanup feature.

## Reproduce

Build the PBO with `tools/build.ps1`, then run the suite against
an installed Windows DayZ dedicated server:

```powershell
./tools/ci/gun-cleanup-smoke.ps1 -ServerRoot 'C:\dayz-server'
./tools/ci/gun-cleanup-smoke.ps1 -SelfTest
```

The runner tests `server`, `round_end`, `player_death`, an existing config
without the setting, and an invalid mode. Both fallback cases must behave
as `round_end`, and loading must leave the operator's config file unchanged.
Use `-Mode` to select one case. The runner creates isolated missions and
profiles under `build/gun-cleanup-smoke/`, starts hidden servers on separate
localhost ports, and stops only processes it started. Use `-Port` to choose
the first port. It requires no connected clients and loads only the PBO, with
signature verification and BattlEye disabled for this local test server.
Logs remain alongside the profile.

The mission uses a simulated LIVE round, synthetic players, a real gunshot
damage/death event, and local inventory moves, avoiding client inventory
acknowledgements. It directly registers the test corpse
with the cleanup service. It does not verify the PlayerBase hook's hand-gun
capture and forwarding into RoundEngine, a real client's death or network
replication, nor wait for native Central Economy despawn; the server-mode
probe verifies the mod leaves those guns alone.

## Coverage

The earlier v0.1.32-based test build passed all three modes. The expanded
suite below exercises the signed release candidate on this PC.

The pure boot fixtures cover config defaults, accepted modes, invalid-value
fallback, round-start versus round-end gun sweep decisions, and the exact
ten-second death deadline. The two new cleanup decision fixtures were first
observed failing against unimplemented policy functions on a local server.

The native entity probes cover:

- Releasing a dead player's inventory guns before corpse deletion.
- Gunshot damage kills the synthetic victim and records the attacker.
- No gun cleanup before ten seconds while the simulated round remains LIVE.
- The cleanup service's real one-second timer removes death-mode guns between
  ten and twelve seconds while combat continues; the other modes keep them.
- A gun held by the looter survives both the active-combat interval and cleanup.
- Keeping guns and gun-containing ground containers through countdown.
- Deleting death guns and manually dropped guns at round end in default mode.
- Keeping death timers intact when a round ends before ten seconds.
- Preserving server-mode guns after corpse and round cleanup.
- Protecting guns looted before cleanup and after the round-end sweep queues them.
- Leaving LIVE for ROUNDEND, VOTING (map vote), or IDLE (population loss).

The runner requires every named boot fixture and every scenario assertion,
the expected version's round-engine startup marker, and the correct mode's
completion marker. It fails on compile/runtime errors, FAIL results, missing
checks, or config rewrites. Parser regression tests also run in CI, without
a DayZ installation. Duplicate log lines cannot replace missing checks.
Workshop publication remains a separate action.

## Local results

The signed v0.1.34 candidate passed all five native cases on this PC:

| Configuration | Boot fixtures | Scenario assertions | Result |
| --- | ---: | ---: | --- |
| `server` | 69 | 24 | PASS |
| `round_end` | 69 | 24 | PASS |
| `player_death` | 69 | 25 | PASS |
| Missing setting | 69 | 24 | PASS |
| Invalid setting | 69 | 24 | PASS |

Death-mode deletion was observed at 10.711 seconds. Every case retained the
input config's SHA-256 hash and reported no compile or runtime errors.
The six existing medical regression cases also passed against the signed
candidate, each with all 69 boot fixtures. Both parser self-test suites and
the static gates passed. Logs are retained under the local build directory.

## Cost argument

The added PlayerBase hook performs one hand-item lookup per death. Corpse
registration enumerates the victim's inventory once and drops only firearm
entries. Neither adds work to a per-frame callback or the damage hook.

The existing one-second cleanup timer scans O(q) pending records, where q
includes one additional record per unlooted death gun in mod-managed modes.
Round-end mode retains those records until round end; server mode adds no
gun records. Deletions remain capped by `MaxDeletesPerTick`. Contained-gun
checks happen only when a queued non-gun ground item is due for deletion;
corpse inventory checks happen only when that corpse is due for deletion.

The arena ground query runs once when leaving LIVE, including aborted
rounds, and once at countdown as before. Server and player-death modes skip
the round-end query entirely. A large deletion backlog can extend the
ten-second delay; the smoke runner uses the normal deletion budget of three
and checks a small scenario without a large backlog. This is an event/timer cost
argument, not a measured frame-time bound or a populated-server load test.
