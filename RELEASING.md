# Releasing

The Workshop is the release channel: every publish reaches live servers and
players automatically. Treat every upload like a deploy, because it is one.

## Checklist

1. **Version + changelog.** Bump `DmVersion.VERSION` and `config.cpp`
   `version` together (CI enforces lockstep). Move CHANGELOG `[Unreleased]`
   into a `[X.Y.Z]` section.
2. **Static gates.** `bash tools/ci/checks.sh` clean.
3. **Build signed.** `.\tools\build.ps1` with signing enabled. Confirm the
   PBO, `.bisign`, `keys/*.bikey`, `meta.cpp`, and `preview.png` are all in
   `build/@SentinelDeathmatch/`.
4. **Boot smoke.** A dedicated-server boot with the built folder: zero
   compile errors, all `[DM] fixture` lines PASS, `round engine started`.
   The packer accepting the build proves nothing - only a boot does.
5. **Tag after smoke.** `git tag -a vX.Y.Z` once the boot passes, push with
   tags.
6. **Publish.** Upload `build/@SentinelDeathmatch` over the existing
   Workshop item with a change note naming the version. Keep the
   description under 7,900 characters (Steam's 8,000 cap rejects silently;
   the build script gates this).
7. **Verify distribution.** After Steam processes the update, spot-check a
   server running the mod with the DZSA checker
   (`https://dayzsalauncher.com/api/v1/query/<ip>/<queryPort>`): the mod
   must resolve with its Workshop id.

## Hard-won rules

- **`meta.cpp` must carry the item's own `publishedid`.** The very first
  upload of a new item necessarily ships `publishedid = 0` (the id doesn't
  exist yet) - follow it immediately with a second upload carrying the real
  id, or launchers like DZSA cannot index servers running the mod and will
  refuse to list them.
- **Config schema is append-only across releases.** Operators' JSON files
  are never rewritten by updates; renamed fields silently revert to
  defaults for existing servers. Add fields, never rename.
- **A new-item publish may get auto-flagged by Steam moderation** (false
  positive). A trivial content update forces a re-scan and typically clears
  it; appeal via Steam Support only if that fails.
- Servers commonly auto-update mods on a timer: assume a publish is LIVE on
  real servers within the hour. Do not publish anything that has not
  boot-smoked.
