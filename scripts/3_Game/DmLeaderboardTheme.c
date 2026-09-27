class DmLeaderboardTheme
{
	string BrandName = "COMMUNITY";
	string Subtitle = "DEATHMATCH";
	string Footer = "";
	string LogoPath = "";
	bool ShowLogo = true;
	int AccentR = 59;
	int AccentG = 130;
	int AccentB = 246;
	int SurfaceR = 7;
	int SurfaceG = 8;
	int SurfaceB = 10;
	float PanelOpacity = 0.97;

	int Accent()
	{
		return ARGB(255, AccentR, AccentG, AccentB);
	}

	int Surface()
	{
		int panelAlpha = ClampByte((int)(PanelOpacity * 255.0));
		return ARGB(panelAlpha, ClampByte(SurfaceR), ClampByte(SurfaceG), ClampByte(SurfaceB));
	}

	void Validate()
	{
		AccentR = ClampByte(AccentR);
		AccentG = ClampByte(AccentG);
		AccentB = ClampByte(AccentB);
		SurfaceR = ClampByte(SurfaceR);
		SurfaceG = ClampByte(SurfaceG);
		SurfaceB = ClampByte(SurfaceB);
		if (PanelOpacity < 0.0) PanelOpacity = 0.0;
		if (PanelOpacity > 1.0) PanelOpacity = 1.0;
		BrandName = LimitString(BrandName, 32);
		Subtitle = LimitString(Subtitle, 48);
		Footer = LimitString(Footer, 128);
		LogoPath = LimitString(LogoPath, 160);
		if (LogoPath.IndexOf(":") >= 0 || LogoPath.IndexOf("..") >= 0) LogoPath = "";
	}

	static string SafeFileName(string value)
	{
		if (value.Length() < 6 || value.Length() > 80) return "leaderboard-theme.json";
		if (value.Substring(value.Length() - 5, 5) != ".json") return "leaderboard-theme.json";
		string allowed = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
		for (int fileChar = 0; fileChar < value.Length() - 5; fileChar++)
		{
			if (allowed.IndexOf(value.Substring(fileChar, 1)) < 0) return "leaderboard-theme.json";
		}
		return value;
	}

	static DmLeaderboardTheme Load(string filePath)
	{
		DmLeaderboardTheme loadedTheme = new DmLeaderboardTheme();
		// Server validates once; clients also validate the received configuration.
		if (filePath != "" && FileExist(filePath))
		{
			string loadError;
			if (!JsonFileLoader<DmLeaderboardTheme>.LoadFile(filePath, loadedTheme, loadError))
			{
				Print("[DM] invalid theme; using neutral defaults: " + loadError);
				loadedTheme = new DmLeaderboardTheme();
			}
		}
		loadedTheme.Validate();
		return loadedTheme;
	}

	static int ClampByte(int value)
	{
		if (value < 0) return 0;
		if (value > 255) return 255;
		return value;
	}

	static string LimitString(string value, int maxLength)
	{
		value.Replace("\r", " ");
		value.Replace("\n", " ");
		value.Replace("\t", " ");
		if (value.Length() <= maxLength) return value;
		return value.Substring(0, maxLength);
	}

	static void SelfTest()
	{
		DmLeaderboardTheme probe = new DmLeaderboardTheme();
		probe.AccentR = -2;
		probe.AccentG = 999;
		probe.PanelOpacity = 2;
		probe.BrandName = "one\ntwo";
		probe.LogoPath = "https://example.invalid/logo.paa";
		probe.Validate();
		bool clampOk = probe.AccentR == 0 && probe.AccentG == 255 && probe.PanelOpacity == 1 && probe.BrandName == "one two" && probe.LogoPath == "";
		Print("[DM] fixture DmLeaderboardTheme validation: expected=1 got=" + clampOk.ToString() + " " + DmFixture.Verdict(clampOk));
		bool pathOk = SafeFileName("community-theme.json") == "community-theme.json";
		pathOk = pathOk && SafeFileName("../theme.json") == "leaderboard-theme.json" && SafeFileName("C:\\theme.json") == "leaderboard-theme.json";
		pathOk = pathOk && SafeFileName("theme.txt") == "leaderboard-theme.json" && SafeFileName("") == "leaderboard-theme.json";
		Print("[DM] fixture DmLeaderboardTheme file name: expected=1 got=" + pathOk.ToString() + " " + DmFixture.Verdict(pathOk));
	}
}
