# Medical modes validation

The medical mode is independent of spawn protection and survival-pressure
removal. Existing configs missing the new fields use `bandages`. Regen uses
one server-wide deadline in every round phase; it does not reset on damage
or give every respawning body its own timer. It can offset nonlethal zone
soft-wall damage, but does not prevent a zone kill or other lethal damage.

## Configuration

```json
{
  "MedicalMode": "regen",
  "RegenIntervalSeconds": 5,
  "RegenHealthPerTick": 5
}
```

`MedicalMode` accepts `bandages` and `regen` (lowercase). Other values fall
back to `bandages`. Intervals clamp to 0.5-3600 seconds; HP clamps to
0.1-100 per tick. Timing uses the existing 500 ms timer, so fractional
intervals round up to a timer tick and server stalls can delay healing.
Missed intervals do not accumulate. Restart the server after editing its
`$profile:SentinelDeathmatch/config.json`; existing config files are not
rewritten. Leave the current loadout's medical items in place in either mode.

Regen heals living players only, up to maximum health, and removes bleeding
sources at each due tick even if health is full. It does not restore blood,
shock, or broken legs, and it does not grant damage immunity. Blood lost
before a tick still needs its normal recovery.

## Reproducible engine smoke

Run the automated suite on a Windows PC with the dedicated server installed:

```powershell
./tools/ci/test-medical.ps1 -ServerRoot '<DayZ dedicated server installation>'
```

The command builds the current checkout, boots six isolated local-only cases,
and exits unsuccessfully for failed/missing assertions, compile/runtime errors,
an incomplete boot, or a timeout. It preserves profiles, logs, and a JSON
result under `build/medical-tests/`. Each case tests the real medical tick
against independently specified configuration expectations: missing fields,
default Regen, tuned 2-second/7-HP Regen, an unknown mode, and both numeric
clamp extremes. The 3600-second case advances explicit times, so it does not
require waiting an hour. Run `-SkipBuild` only when intentionally testing the
existing PBO. CI runs `-SelfTest` to check the runner's rejection of invalid
logs; full engine tests require the local installed server.

`tools/ci/medical-smoke/init.c` is a standalone test mission that creates
temporary identityless player bodies. It is separate from production boot
fixtures. Run only on an isolated local dedicated server with no clients.

1. Run `bash tools/ci/checks.sh` and `./tools/build.ps1`.
2. Prefer the automated runner above, which also creates the expectations
   file used by the mission. For manual runs, create a disposable
   `dayzOffline.medical.chernarusplus` mission folder
   and copy the smoke `init.c` into it.
3. Create a fresh profile. For the compatibility run, put `{}` in
   `SentinelDeathmatch/config.json`. For the Regen run, use the JSON above.
   Supply `medical-expectations.json` in the profile root with the keys
   `Mode`, `IntervalSeconds`, and `HealthPerTick` containing the expected
   loaded values (for example `regen`, `5`, and `5`).
4. Launch a local-only dedicated server with `-ip=127.0.0.1`, an unused
   port, that profile, `-mission=<absolute smoke mission folder>`, and
   `-mod=<absolute build/@SentinelDeathmatch folder>`. Restrict access with a
   test-only passphrase. Set `verifySignatures` and `BattlEye` to zero, and
   enable `-doLogs -NO_GUI`.
5. Require every `[DM] fixture` and `[DM-MEDICAL]` assertion to pass, the
   `smoke complete` marker, and zero script compile/runtime errors. Stop
   the process between runs. Keep all mission persistence in the disposable
   folder; do not reuse a live server's profile or mission.

The smoke checks real health and bleeding APIs, deadline initialization,
no early healing, a single heal after a stall, no catch-up burst, maximum
health, bleed removal at full health, unchanged blood/shock, dead-player
exclusion, and the old-config Bandages fallback. It also reports the mean
time of 100 due calls over 60 wounded/bleeding bodies; setup and deletion
are outside the timed region. It calls the production medical tick with
explicit times, rather than requiring connected clients or waiting minutes.

## Verification record (2026-10-09)

The automated suite passed on this PC using the signed v0.1.33 build:

| Case | Loaded mode | Interval / HP | Result |
| --- | --- | --- | --- |
| Missing medical fields | bandages | 5 s / 5 HP | PASS |
| Regen defaults | regen | 5 s / 5 HP | PASS |
| Tuned Regen | regen | 2 s / 7 HP | PASS |
| Unknown mode | bandages | 5 s / 5 HP | PASS |
| Low interval / high HP | regen | 0.5 s / 100 HP | PASS |
| High interval / low HP | regen | 3600 s / 0.1 HP | PASS |

Every case passed all 66 boot fixtures and its exact medical assertions (5
for Bandages, 12 for Regen), with no script errors. All six operator-config
hashes remained unchanged. The runner's 17 parser regression checks also
passed under PowerShell 7 and Windows PowerShell 5.1. Independent review of
the automated runner and parameterized mission found no blockers.

Earlier incremental validation:

- Red: dedicated-server boot produced the expected failures for both new
  regen helper fixtures with no-op implementations.
- Green: 66/66 boot fixtures passed; round engine started, no compile errors.
- Targeted Regen smoke: all ten behavioral assertions passed, plus the
  60-body benchmark precondition. Bandages mode also passed the old-config
  health/bleeding preservation check.
- Static gates, six example JSON files, and `git diff --check` passed.
- Independent code review reported no actionable findings.

The final 60-body Regen run measured 230.045 us mean and 1001.36 us maximum
observed per due `TickMedical` call over 100 samples. The Bandages run's
medical calls were below the timer's resolution (reported 0 us). This is
the added medical work only, not the full round tick. It ran on the local
DayZ 1.29 Windows dedicated server with identityless bodies; connected-client
replication cost was not measured. Before this change, ongoing medical
healing added no work. With the default Bandages mode there is only a cached
mode check and return every 500 ms. Regen adds one bounded player scan per
configured interval (5 seconds by default, 500 ms minimum), never an
event-driven or per-frame loop, and performs no synced health write for a
healthy player.

Final behavioral output:

```text
[DM-MEDICAL] bleeding precondition PASS
[DM-MEDICAL] no early heal PASS
[DM-MEDICAL] configured HP and bleed clearing PASS
[DM-MEDICAL] blood and shock unchanged PASS
[DM-MEDICAL] stall heals once PASS
[DM-MEDICAL] no catch-up burst PASS
[DM-MEDICAL] maximum HP cap PASS
[DM-MEDICAL] full HP bleeding precondition PASS
[DM-MEDICAL] full HP still clears bleeds PASS
[DM-MEDICAL] dead player remains dead PASS
[DM-MEDICAL] benchmark 60 bodies PASS
[DM-MEDICAL] smoke complete
```

These are server-side checks. Client HUD appearance and medical animations
were not tested; no Workshop release was made.
