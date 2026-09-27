// Server-only profile access stays out of the DTO and every hot path.
class DmLeaderboardThemeStore
{
	private static ref DmLeaderboardTheme s_Theme;

	static DmLeaderboardTheme Get()
	{
		if (!s_Theme)
		{
			string fileName = DmConfig.GetInstance().GetLeaderboardThemeFile();
			s_Theme = DmLeaderboardTheme.Load(DmConfig.CONFIG_DIR + "/" + fileName);
		}
		return s_Theme;
	}
}
