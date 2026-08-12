# Contributing

Thanks for your interest in Sentinel Deathmatch. This guide covers the
engineering standards for the project. PRs that don't follow them will be
asked to revise, so it's worth the five-minute read.

## Ground rules

- **Performance is the product.** This mod is built for 20-60 players
  shooting in one area. Nothing lands on a hot path without a measurement.
- **No per-frame work.** All recurring work rides timers (the round engine's
  500 ms tick) or one-shot `CallLater` deferrals. `OnUpdate` overrides are
  not accepted.
- **One chain link per modded class.** Exactly one `modded class PlayerBase`
  block (`scripts/4_World/capture/DmPlayerHook.c`) and one
  `modded class MissionServer` block (`scripts/5_Mission/DmMissionServer.c`)
  exist in this codebase. New event needs go INTO those blocks, which call
  free functions elsewhere. Never add a second block for the same type.
- **Bail before allocating.** Event-driven code checks its cheapest gate
  first and returns before constructing anything.
- **Config schema is append-only.** JSON loaders never overwrite existing
  fields in an operator's file. Never rename or repurpose a config field;
  add a new one and mark the old `DEPRECATED` in a comment.
- **ASCII-only source files.** Non-ASCII bytes in `.c` files break the
  in-engine tokenizer in ways the packer does not catch.

## Enforce Script pitfalls (the packer will NOT save you)

A green PBO build proves nothing: `MakePbo` packages script that does not
compile in-engine. Only a server boot proves the code compiles. Three known
killers, all of which pass the packer:

1. **No leading-`+` string continuation.** A multi-line string concatenation
   must put the `+` at the END of a line, never the start of the next.
2. **Locals scope to the whole method, not the enclosing block.** Two sibling
   `for` loops each declaring `int i` fail the in-engine compile with
   `Multiple declaration` - and the failure can surface as an unrelated-looking
   crash far from the real bug. Every local in a function body needs a unique
   name across the whole function.
3. **Non-ASCII bytes** (em-dashes, curly quotes) in source: see above.

CI enforces 1 and 3 statically. Nothing enforces 2 except a boot.

## Testing standards

There is no unit-test framework for Enforce Script. This project's answer is
**boot fixtures**:

- Every subsystem class ships a `static void SelfTest()` registered in
  `DmRunSelfTests()` (`scripts/5_Mission/DmMissionServer.c`).
- Fixtures print one grep-parseable line each to the server script log:
  `[DM] fixture <name>: expected=<x> got=<y> PASS|FAIL`
- Fixtures must be pure and self-contained: no network, no filesystem writes
  outside the profile dir, no dependence on players being connected.
- Design for testability: put decision logic in static/pure functions
  (see `DmPhase.Next`) so fixtures can cover it without a live round.

**Definition of done for a PR:**

1. CI green (static checks + secret scan).
2. A local dedicated-server boot with the mod loaded, showing in the script
   log: zero compile errors, all `[DM] fixture` lines `PASS` (include the
   fixture output in the PR description).
3. If the diff touches the round-engine tick, the PlayerBase hook block, or
   anything else event-storm-shaped: a before/after cost statement (measured
   µs/frame bound on a local server, or a written argument for why the path
   cannot be hot). "It felt fine" is not a measurement.
4. New config fields documented in the example config and clamped in
   `ClampLoadedValues()` if a bad value could break the round loop.

## Local build

Requirements: Windows, [Mikero DePboTools](https://mikero.bytex.digital/)
(`MakePbo.exe`), optionally DayZ Tools (Steam) for signing.

```
.\tools\build.ps1
```

Output: `build/@SentinelDeathmatch/addons/sentinel_dm.pbo`. Load on a local
DayZ dedicated server via `-serverMod=@SentinelDeathmatch` (Phase 0 is
server-side only; client UI phases will move the mod to `-mod=`).

## Style

- Follow the surrounding code. Tabs, brace placement, and naming match what
  is already in the file you are editing.
- Comments state constraints the code cannot show (why a gate is ordered
  first, why a surface was avoided) - not narration of what the next line does.
- Class prefix is `Dm`. RPC ids live only in `scripts/3_Game/DmNet.c`.

## Licensing of contributions

By submitting a PR you agree that your contribution is licensed under the
project's license (see `LICENSE.md`) and that you have the right to submit
it. Do not submit code copied from other mods regardless of their license -
this project is clean-room by policy: formats and protocols may be
interoperated with; other people's code may not be copied. Credit prior art
in `docs/CREDITS.md` when a design idea is learned from a readable source.
