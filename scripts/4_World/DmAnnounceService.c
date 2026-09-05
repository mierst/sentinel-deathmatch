// Rotating server announcements (Discord invite, house rules, event
// notices) and the one-shot welcome line.
//
// Driven from the round engine's 500 ms tick, so it costs one integer
// compare per tick and nothing per frame. The schedule is relative to the
// first player of a session: an empty server arms nothing, the first line
// fires one full interval after somebody is actually there to read it, and
// the cursor walks the list in order so multi-line configs rotate evenly.
class DmAnnounceService
{
	private static ref DmAnnounceService s_Instance;

	private float m_NextAt = 0; // engine seconds; 0 = not armed
	private int m_Cursor = 0;

	static DmAnnounceService GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new DmAnnounceService();
		}
		return s_Instance;
	}

	// Boot line so an operator can see from the log that the list parsed
	// (the JSON loader silently yields an empty array on a malformed one).
	void LogBootSummary()
	{
		DmConfig cfg = DmConfig.GetInstance();
		int lineCount = cfg.GetAnnouncementCount();
		int interval = cfg.GetAnnouncementIntervalSeconds();
		string welcomeState = "off";
		if (cfg.GetWelcomeMessage() != "") welcomeState = "on";
		if (interval <= 0 || lineCount == 0)
		{
			Print("[DM] announcements: off (" + lineCount.ToString() + " lines, interval " + interval.ToString() + " s), welcome " + welcomeState);
			return;
		}
		Print("[DM] announcements: " + lineCount.ToString() + " lines every " + interval.ToString() + " s (" + cfg.GetAnnouncementColor() + "), welcome " + welcomeState);
	}

	void Tick(int playerCount, float nowSeconds)
	{
		DmConfig cfg = DmConfig.GetInstance();
		int interval = cfg.GetAnnouncementIntervalSeconds();
		int lineCount = cfg.GetAnnouncementCount();
		if (interval <= 0 || lineCount == 0) return;

		if (playerCount <= 0)
		{
			m_NextAt = 0;
			return;
		}

		if (m_NextAt == 0)
		{
			m_NextAt = nowSeconds + interval;
			return;
		}
		if (nowSeconds < m_NextAt) return;

		m_NextAt = nowSeconds + interval;
		string line = cfg.GetAnnouncement(m_Cursor);
		m_Cursor = NextCursor(m_Cursor, lineCount);
		if (line == "") return;
		DmNetServer.GetInstance().SendChatAll(line, cfg.GetAnnouncementColor());
		if (cfg.IsDebug()) Print("[DM] announcement: " + line);
	}

	// Called (deferred) on a player's first connect of the session; the
	// dedup lives in DmRoundEngine.OnPlayerJoined so respawns never repeat
	// it. A player who left during the delay simply is not found.
	void SendWelcome(PlayerIdentity ident)
	{
		if (!ident) return;
		string welcome = DmConfig.GetInstance().GetWelcomeMessage();
		if (welcome == "") return;
		DmNetServer.GetInstance().SendChatTo(ident, welcome);
	}

	// Pure rotation step so the fixture can pin wrap-around behaviour.
	static int NextCursor(int cursor, int lineCount)
	{
		if (lineCount <= 0) return 0;
		int next = cursor + 1;
		if (next >= lineCount) next = 0;
		return next;
	}

	static void SelfTest()
	{
		int rotOk = 1;
		if (DmAnnounceService.NextCursor(0, 3) != 1) rotOk = 0;
		if (DmAnnounceService.NextCursor(2, 3) != 0) rotOk = 0;
		if (DmAnnounceService.NextCursor(0, 1) != 0) rotOk = 0;
		if (DmAnnounceService.NextCursor(5, 0) != 0) rotOk = 0;
		Print("[DM] fixture DmAnnounceService rotation: expected=1 got=" + rotOk.ToString() + " " + DmFixture.Verdict(rotOk == 1));
	}
}
