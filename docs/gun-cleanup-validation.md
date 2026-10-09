# Gun cleanup validation

Local development validation, 2026-10-09, based on main v0.1.32.

## Reproduce

Build the development PBO with `tools/build.ps1`, then run each mode against
an installed Windows DayZ dedicated server:

```powershell
foreach ($mode in @('round_end', 'player_death', 'server')) {
    ./tools/ci/gun-cleanup-smoke.ps1 -ServerRoot 'C:\dayz-server' -Mode $mode
}
```

The runner creates an isolated mission and profile under
`build/gun-cleanup-smoke/`, starts a hidden server on port 24912, and stops
only the process it started. Use `-Port` to choose another port. It requires
no connected clients and loads only the development Deathmatch PBO, with
signature verification and BattlEye disabled for this local test server.
Logs remain alongside the profile.

The mission uses synthetic players and local inventory moves, avoiding
client inventory acknowledgements. It directly registers the test corpse
with the cleanup service. It does not test a real client's death or network
replication, nor wait for native Central Economy despawn; the server-mode
probe verifies the mod leaves those guns alone.

## Coverage

The v0.1.32-based unsigned build booted successfully in all three modes with
65 passing fixtures per run and no script errors. Round-end mode passed 15
entity probes; player-death and server modes each passed 16. Static gates,
example JSON parsing, and `git diff --check` passed.

The pure boot fixtures cover config defaults, accepted modes, invalid-value
fallback, round-start versus round-end gun sweep decisions, and the exact
ten-second death deadline. The two new cleanup decision fixtures were first
observed failing against unimplemented policy functions on a local server.

The native entity probes cover:

- Releasing a dead player's inventory guns before corpse deletion.
- Keeping guns and gun-containing ground containers through countdown.
- Deleting death guns and manually dropped guns at round end in default mode.
- Keeping death timers intact when a round ends before ten seconds.
- Preserving server-mode guns after corpse and round cleanup.
- Protecting guns looted before cleanup and after the round-end sweep queues them.
- Leaving LIVE for ROUNDEND, VOTING (map vote), or IDLE (population loss).

The runner requires at least 65 passing boot fixtures and exactly 15 probes
for round-end mode or 16 for each other mode. It fails on any fixture/probe
FAIL or script error, or if its mode's final completion marker is missing.
Static gates, example JSON parsing, and an
unsigned PBO build are also checked. Signing and Workshop publication are
separate release work.

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
ten-second delay; the smoke runner raises the budget to 100 to isolate
deadline behavior from backlog behavior. This is an event/timer cost
argument, not a measured frame-time bound or a populated-server load test.
