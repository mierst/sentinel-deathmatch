// Per-round scoring: kills, deaths, streaks.
class DmScoreEntry
{
	string PlayerName = "";
	int Kills = 0;
	int Deaths = 0;
	int Streak = 0;
	int BestStreak = 0;
}

class DmScoreService
{
	private static ref DmScoreService s_Instance;

	private ref map<string, ref DmScoreEntry> m_Scores = new map<string, ref DmScoreEntry>;

	static DmScoreService GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new DmScoreService();
		}
		return s_Instance;
	}

	void Reset()
	{
		m_Scores.Clear();
	}

	DmScoreEntry GetOrCreate(string playerId, string playerName)
	{
		DmScoreEntry entry = m_Scores.Get(playerId);
		if (!entry)
		{
			entry = new DmScoreEntry();
			entry.PlayerName = playerName;
			m_Scores.Set(playerId, entry);
		}
		return entry;
	}

	// Returns the killer's new streak (0 for suicide/environment deaths).
	int RegisterKill(string killerId, string killerName, string victimId, string victimName)
	{
		DmScoreEntry victimEntry = GetOrCreate(victimId, victimName);
		victimEntry.Deaths = victimEntry.Deaths + 1;
		victimEntry.Streak = 0;

		if (killerId == "" || killerId == victimId) return 0;

		DmScoreEntry killerEntry = GetOrCreate(killerId, killerName);
		killerEntry.Kills = killerEntry.Kills + 1;
		killerEntry.Streak = killerEntry.Streak + 1;
		if (killerEntry.Streak > killerEntry.BestStreak)
		{
			killerEntry.BestStreak = killerEntry.Streak;
		}
		return killerEntry.Streak;
	}

	int LeaderScore()
	{
		int best = 0;
		for (int scanIdx = 0; scanIdx < m_Scores.Count(); scanIdx++)
		{
			DmScoreEntry entry = m_Scores.GetElement(scanIdx);
			if (entry.Kills > best) best = entry.Kills;
		}
		return best;
	}

	string LeaderId()
	{
		int best = -1;
		string bestId = "";
		for (int scanIdx = 0; scanIdx < m_Scores.Count(); scanIdx++)
		{
			DmScoreEntry entry = m_Scores.GetElement(scanIdx);
			if (entry.Kills > best)
			{
				best = entry.Kills;
				bestId = m_Scores.GetKey(scanIdx);
			}
		}
		return bestId;
	}

	// Round-end summary to the server log (the client scoreboard is a later
	// phase; this keeps rounds auditable meanwhile).
	void PrintSummary()
	{
		Print("[DM] round summary (" + m_Scores.Count().ToString() + " players):");
		for (int rowIdx = 0; rowIdx < m_Scores.Count(); rowIdx++)
		{
			DmScoreEntry entry = m_Scores.GetElement(rowIdx);
			Print("[DM]   " + entry.PlayerName + " K:" + entry.Kills.ToString() + " D:" + entry.Deaths.ToString() + " best streak:" + entry.BestStreak.ToString());
		}
	}

	static void SelfTest()
	{
		DmScoreService probe = new DmScoreService();
		int streakOne = probe.RegisterKill("k1", "Killer", "v1", "Victim");
		int streakTwo = probe.RegisterKill("k1", "Killer", "v2", "Victim2");
		int killOk = 1;
		if (streakOne != 1 || streakTwo != 2) killOk = 0;
		if (probe.LeaderScore() != 2) killOk = 0;
		if (probe.LeaderId() != "k1") killOk = 0;
		Print("[DM] fixture DmScoreService kill+streak: expected=1 got=" + killOk.ToString() + " " + DmFixture.Verdict(killOk == 1));

		int suicideStreak = probe.RegisterKill("v2", "Victim2", "v2", "Victim2");
		int suicideOk = 1;
		if (suicideStreak != 0) suicideOk = 0;
		DmScoreEntry v2Entry = probe.GetOrCreate("v2", "Victim2");
		if (v2Entry.Kills != 0) suicideOk = 0;
		if (v2Entry.Deaths != 2) suicideOk = 0;
		Print("[DM] fixture DmScoreService suicide no credit: expected=1 got=" + suicideOk.ToString() + " " + DmFixture.Verdict(suicideOk == 1));

		int streakResetOk = 1;
		probe.RegisterKill("k2", "K2", "k1", "Killer");
		DmScoreEntry k1Entry = probe.GetOrCreate("k1", "Killer");
		if (k1Entry.Streak != 0) streakResetOk = 0;
		if (k1Entry.BestStreak != 2) streakResetOk = 0;
		probe.Reset();
		if (probe.LeaderScore() != 0) streakResetOk = 0;
		Print("[DM] fixture DmScoreService streak reset: expected=1 got=" + streakResetOk.ToString() + " " + DmFixture.Verdict(streakResetOk == 1));
	}
}
