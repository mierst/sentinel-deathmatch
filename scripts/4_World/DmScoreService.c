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

	// Round scores reset every round; session scores accumulate across rounds
	// for the in-game session leaderboard (cleared only on server restart).
	private ref map<string, ref DmScoreEntry> m_Scores = new map<string, ref DmScoreEntry>;
	private ref map<string, ref DmScoreEntry> m_SessionScores = new map<string, ref DmScoreEntry>;

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

	private DmScoreEntry GetOrCreateIn(map<string, ref DmScoreEntry> scoreMap, string playerId, string playerName)
	{
		DmScoreEntry entry = scoreMap.Get(playerId);
		if (!entry)
		{
			entry = new DmScoreEntry();
			entry.PlayerName = playerName;
			scoreMap.Set(playerId, entry);
		}
		return entry;
	}

	DmScoreEntry GetOrCreate(string playerId, string playerName)
	{
		return GetOrCreateIn(m_Scores, playerId, playerName);
	}

	// Deaths with nobody to credit (suicide, zone enforcement) cost the
	// victim a kill on top of the death - going negative is allowed.
	void RegisterPenalty(string playerId, string playerName)
	{
		DmScoreEntry roundEntry = GetOrCreateIn(m_Scores, playerId, playerName);
		roundEntry.Kills = roundEntry.Kills - 1;
		DmScoreEntry sessionEntry = GetOrCreateIn(m_SessionScores, playerId, playerName);
		sessionEntry.Kills = sessionEntry.Kills - 1;
	}

	// Returns the killer's new ROUND streak (0 for suicide/environment deaths).
	int RegisterKill(string killerId, string killerName, string victimId, string victimName)
	{
		DmScoreEntry victimEntry = GetOrCreateIn(m_Scores, victimId, victimName);
		victimEntry.Deaths = victimEntry.Deaths + 1;
		victimEntry.Streak = 0;
		DmScoreEntry victimSession = GetOrCreateIn(m_SessionScores, victimId, victimName);
		victimSession.Deaths = victimSession.Deaths + 1;
		victimSession.Streak = 0;

		if (killerId == "" || killerId == victimId) return 0;

		DmScoreEntry killerSession = GetOrCreateIn(m_SessionScores, killerId, killerName);
		killerSession.Kills = killerSession.Kills + 1;
		killerSession.Streak = killerSession.Streak + 1;
		if (killerSession.Streak > killerSession.BestStreak)
		{
			killerSession.BestStreak = killerSession.Streak;
		}

		DmScoreEntry killerEntry = GetOrCreateIn(m_Scores, killerId, killerName);
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

	string LeaderName()
	{
		DmScoreEntry leaderEntry = m_Scores.Get(LeaderId());
		if (!leaderEntry) return "";
		return leaderEntry.PlayerName;
	}

	// Rows sorted by kills desc, tab-separated fields, newline rows:
	// name\tkills\tdeaths\tbeststreak
	static string BuildRowsBlobFor(map<string, ref DmScoreEntry> scoreMap)
	{
		array<string> ids = new array<string>;
		for (int keyIdx = 0; keyIdx < scoreMap.Count(); keyIdx++)
		{
			ids.Insert(scoreMap.GetKey(keyIdx));
		}

		// Selection sort by kills desc - trivially small N (players/round).
		for (int sortIdx = 0; sortIdx < ids.Count(); sortIdx++)
		{
			int maxAt = sortIdx;
			for (int probeIdx = sortIdx + 1; probeIdx < ids.Count(); probeIdx++)
			{
				if (scoreMap.Get(ids[probeIdx]).Kills > scoreMap.Get(ids[maxAt]).Kills)
				{
					maxAt = probeIdx;
				}
			}
			if (maxAt != sortIdx)
			{
				string swapId = ids[sortIdx];
				ids[sortIdx] = ids[maxAt];
				ids[maxAt] = swapId;
			}
		}

		string blob = "";
		for (int rowIdx = 0; rowIdx < ids.Count(); rowIdx++)
		{
			DmScoreEntry rowEntry = scoreMap.Get(ids[rowIdx]);
			if (rowIdx > 0) blob = blob + "\n";
			blob = blob + rowEntry.PlayerName + "\t" + rowEntry.Kills.ToString() + "\t" + rowEntry.Deaths.ToString() + "\t" + rowEntry.BestStreak.ToString();
		}
		return blob;
	}

	string BuildRowsBlob() { return BuildRowsBlobFor(m_Scores); }
	string BuildSessionRowsBlob() { return BuildRowsBlobFor(m_SessionScores); }

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

		// Session totals must survive the round reset above.
		int sessionOk = 1;
		if (probe.BuildRowsBlob() != "") sessionOk = 0;
		string sessionBlob = probe.BuildSessionRowsBlob();
		if (sessionBlob == "") sessionOk = 0;
		if (sessionBlob.IndexOf("Killer\t2") < 0) sessionOk = 0;
		Print("[DM] fixture DmScoreService session survives reset: expected=1 got=" + sessionOk.ToString() + " " + DmFixture.Verdict(sessionOk == 1));

		// Penalty deaths cost a kill and may go negative, in round AND session.
		DmScoreService penaltyProbe = new DmScoreService();
		penaltyProbe.RegisterKill("", "", "p1", "Loner");
		penaltyProbe.RegisterPenalty("p1", "Loner");
		int penaltyOk = 1;
		DmScoreEntry p1Entry = penaltyProbe.GetOrCreate("p1", "Loner");
		if (p1Entry.Kills != -1) penaltyOk = 0;
		if (p1Entry.Deaths != 1) penaltyOk = 0;
		if (penaltyProbe.BuildSessionRowsBlob().IndexOf("Loner\t-1") < 0) penaltyOk = 0;
		Print("[DM] fixture DmScoreService penalty negative: expected=1 got=" + penaltyOk.ToString() + " " + DmFixture.Verdict(penaltyOk == 1));
	}
}
