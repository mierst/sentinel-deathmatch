// Server-only profile access stays out of every hot path. Each screen receives
// its own resolved copy; a bad override must never mutate shared defaults.
class DmLeaderboardThemeStore
{
	private static ref DmLeaderboardTheme s_Theme;
	private static ref DmLeaderboardTheme s_VoteTheme;

	static DmLeaderboardTheme Copy(DmLeaderboardTheme source)
	{
		DmLeaderboardTheme target = new DmLeaderboardTheme();
		if (!source) return target;
		target.BrandName = source.BrandName;
		target.Subtitle = source.Subtitle;
		target.Footer = source.Footer;
		target.LogoPath = source.LogoPath;
		target.ShowLogo = source.ShowLogo;
		target.AccentR = source.AccentR;
		target.AccentG = source.AccentG;
		target.AccentB = source.AccentB;
		target.SurfaceR = source.SurfaceR;
		target.SurfaceG = source.SurfaceG;
		target.SurfaceB = source.SurfaceB;
		target.PanelOpacity = source.PanelOpacity;
		return target;
	}

	static DmLeaderboardTheme ApplyJson(DmLeaderboardTheme inherited, string json, out bool valid)
	{
		DmLeaderboardTheme candidate = Copy(inherited);
		JsonSerializer serializer = new JsonSerializer();
		string error;
		// Deserialize into an initialized copy: absent scalar keys retain the
		// inherited value, while false, zero and empty strings are real values.
		valid = serializer.ReadFromString(candidate, json, error);
		if (!valid || !candidate)
		{
			valid = false;
			return Copy(inherited);
		}
		candidate.Validate();
		return candidate;
	}

	static DmLeaderboardTheme LoadLayer(DmLeaderboardTheme inherited, string fileName)
	{
		string layerPath = DmConfig.CONFIG_DIR + "/" + fileName;
		if (!FileExist(layerPath)) return Copy(inherited);
		DmLeaderboardTheme candidate = Copy(inherited);
		string error;
		bool loaded = JsonFileLoader<DmLeaderboardTheme>.LoadFile(layerPath, candidate, error);
		if (!loaded || !candidate)
		{
			Print("[DM] invalid UI theme layer " + fileName + "; retaining inherited values: " + error);
			return Copy(inherited);
		}
		candidate.Validate();
		return candidate;
	}

	private static void EnsureLoaded()
	{
		if (s_Theme && s_VoteTheme) return;
		DmConfig config = DmConfig.GetInstance();
		DmLeaderboardTheme builtins = new DmLeaderboardTheme();
		DmLeaderboardTheme shared = LoadLayer(builtins, config.GetThemesFile());
		s_Theme = LoadLayer(shared, config.GetLeaderboardThemeFile());
		s_VoteTheme = LoadLayer(shared, config.GetVoteUIThemeFile());
	}

	static DmLeaderboardTheme Get()
	{
		EnsureLoaded();
		return s_Theme;
	}

	static DmLeaderboardTheme GetVote()
	{
		EnsureLoaded();
		return s_VoteTheme;
	}

	static void SelfTest()
	{
		DmLeaderboardTheme defaults = new DmLeaderboardTheme();
		bool sharedValid;
		DmLeaderboardTheme shared = ApplyJson(defaults, "{\"BrandName\":\"Shared\",\"Footer\":\"Common footer\",\"AccentR\":123,\"PanelOpacity\":0.8}", sharedValid);
		bool screenValid;
		DmLeaderboardTheme screen = ApplyJson(shared, "{\"Subtitle\":\"VOTE\"}", screenValid);
		bool inheritOk = sharedValid && screenValid && screen.BrandName == "Shared" && screen.Footer == "Common footer" && screen.AccentR == 123 && screen.PanelOpacity == 0.8 && screen.Subtitle == "VOTE" && screen.AccentG == defaults.AccentG;
		Print("[DM] fixture DmTheme sparse inheritance: expected=1 got=" + inheritOk.ToString() + " " + DmFixture.Verdict(inheritOk));

		bool explicitValid;
		// Keep adjacent escaped quotes out of a single Enforce source literal.
		string explicitJson = "{\"ShowLogo\":false,\"Footer\":\"";
		explicitJson = explicitJson + "\",\"AccentR\":0,\"PanelOpacity\":0}";
		DmLeaderboardTheme explicitValues = ApplyJson(shared, explicitJson, explicitValid);
		bool explicitOk = explicitValid && !explicitValues.ShowLogo && explicitValues.Footer == "" && explicitValues.AccentR == 0 && explicitValues.PanelOpacity == 0 && explicitValues.BrandName == "Shared";
		Print("[DM] fixture DmTheme explicit empty false zero: expected=1 got=" + explicitOk.ToString() + " " + DmFixture.Verdict(explicitOk));

		bool emptyValid;
		DmLeaderboardTheme empty = ApplyJson(shared, "{}", emptyValid);
		bool isolationOk = emptyValid && empty.BrandName == "Shared" && defaults.BrandName == "COMMUNITY" && shared.Subtitle == "DEATHMATCH" && shared.AccentR == 123;
		empty.BrandName = "Separate";
		isolationOk = isolationOk && shared.BrandName == "Shared";
		Print("[DM] fixture DmTheme empty layer and isolation: expected=1 got=" + isolationOk.ToString() + " " + DmFixture.Verdict(isolationOk));

		bool pathOk = DmLeaderboardTheme.SafeFileName("../shared.json", "themes.json") == "themes.json" && DmLeaderboardTheme.SafeFileName("bad.txt", "vote-ui-theme.json") == "vote-ui-theme.json" && DmLeaderboardTheme.SafeFileName("custom.json", "themes.json") == "custom.json";
		Print("[DM] fixture DmTheme layer filenames: expected=1 got=" + pathOk.ToString() + " " + DmFixture.Verdict(pathOk));
	}

	// Explicit validation harness only: the engine logs JSON parser errors for
	// these intentional bad inputs even when rejection is the expected result.
	static void SelfTestInvalidJson()
	{
		DmLeaderboardTheme shared = new DmLeaderboardTheme();
		shared.BrandName = "Shared";
		shared.Footer = "Common footer";
		shared.AccentR = 123;
		bool malformedValid;
		DmLeaderboardTheme malformed = ApplyJson(shared, "{\"BrandName\":\"Partial mutation\",\"AccentR\":", malformedValid);
		bool malformedOk = !malformedValid && malformed.BrandName == "Shared" && malformed.AccentR == 123 && shared.BrandName == "Shared" && shared.Footer == "Common footer" && shared.ShowLogo;
		Print("[DM] fixture DmTheme malformed layer atomic: expected=1 got=" + malformedOk.ToString() + " " + DmFixture.Verdict(malformedOk));

		bool nullValid;
		DmLeaderboardTheme nullRoot = ApplyJson(shared, "null", nullValid);
		bool arrayValid;
		DmLeaderboardTheme arrayRoot = ApplyJson(shared, "[]", arrayValid);
		bool wrongTypeValid;
		DmLeaderboardTheme wrongType = ApplyJson(shared, "{\"AccentR\":\"bad\"}", wrongTypeValid);
		bool rootOk = !nullValid && !arrayValid && !wrongTypeValid && nullRoot.BrandName == "Shared" && arrayRoot.BrandName == "Shared" && wrongType.AccentR == 123;
		Print("[DM] fixture DmTheme invalid root and field type: expected=1 got=" + rootOk.ToString() + " " + DmFixture.Verdict(rootOk));
	}
}
