// Single source of truth for the mod version.
//
// The version lives in code, not in any config file: JSON loaders do not
// overwrite existing fields when in-code defaults change, so a version read
// from a config file goes stale on servers that keep old files around.
//
// Release procedure:
//   1. Bump VERSION here AND `version` in config.cpp (CI enforces lockstep).
//   2. Move CHANGELOG [Unreleased] -> [X.Y.Z].
//   3. Local server boot smoke (fixtures PASS, no compile errors).
//   4. Commit, tag vX.Y.Z after smoke passes, push with tags.
class DmVersion
{
	static const string VERSION = "0.1.4";
}
