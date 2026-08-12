// Vote tally for zone + preset selection.
//
// Client UI arrives in a later phase; the server side is complete now: options
// come from the validated zone/preset lists, casts arrive via the VOTE_CAST
// RPC (rate-limited, bounds-checked, ignored outside VOTING), and resolution
// picks the highest tally with random tie-break.
class DmVoteService
{
	private static ref DmVoteService s_Instance;

	private bool m_Open = false;
	private ref map<string, int> m_ZoneVoteByPlayer = new map<string, int>;
	private ref map<string, int> m_PresetVoteByPlayer = new map<string, int>;
	private ref map<string, float> m_LastCastAt = new map<string, float>;

	private int m_ActiveZoneEnabledIdx = 0;
	private int m_ActivePresetValidIdx = 0;

	static DmVoteService GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new DmVoteService();
		}
		return s_Instance;
	}

	int GetActivePresetIndex() { return m_ActivePresetValidIdx; }
	int GetActiveZoneIndex() { return m_ActiveZoneEnabledIdx; }

	void OpenVote()
	{
		m_Open = true;
		m_ZoneVoteByPlayer.Clear();
		m_PresetVoteByPlayer.Clear();
		m_LastCastAt.Clear();
	}

	// RPC entry (PlayerBase.OnRPC routes here). All input is untrusted.
	void OnVoteCast(PlayerIdentity sender, int zoneIdx, int presetIdx)
	{
		if (!m_Open || !sender) return;

		string playerId = sender.GetPlainId();
		float nowSeconds = GetGame().GetTickTime();
		float lastAt;
		if (m_LastCastAt.Find(playerId, lastAt))
		{
			if (nowSeconds - lastAt < 1.0) return; // rate limit re-casts
		}
		m_LastCastAt.Set(playerId, nowSeconds);

		if (zoneIdx >= 0 && zoneIdx < DmZonesConfig.GetInstance().GetEnabledCount())
		{
			m_ZoneVoteByPlayer.Set(playerId, zoneIdx);
		}
		if (presetIdx >= 0 && presetIdx < DmLoadoutFactory.GetInstance().GetValidPresetCount())
		{
			m_PresetVoteByPlayer.Set(playerId, presetIdx);
		}
	}

	// Close the vote and arm the winning zone + preset. Empty tally or tie ->
	// random among leaders, so rotation never stalls.
	void Resolve()
	{
		m_Open = false;

		int zoneCount = DmZonesConfig.GetInstance().GetEnabledCount();
		int presetCount = DmLoadoutFactory.GetInstance().GetValidPresetCount();

		m_ActiveZoneEnabledIdx = WinnerFromVotes(m_ZoneVoteByPlayer, zoneCount);
		m_ActivePresetValidIdx = WinnerFromVotes(m_PresetVoteByPlayer, presetCount);

		DmZoneService.GetInstance().SetActiveZone(DmZonesConfig.GetInstance().GetEnabledZone(m_ActiveZoneEnabledIdx));

		int votesCast = m_ZoneVoteByPlayer.Count();
		DmApi.OnVoteResult().Invoke(DmZoneService.GetInstance().GetActiveZoneName(), GetActivePresetName(), votesCast);
	}

	string GetActivePresetName()
	{
		DmPresetData preset = DmLoadoutFactory.GetInstance().GetValidPreset(m_ActivePresetValidIdx);
		if (!preset) return "";
		return preset.Name;
	}

	// Highest tally wins; ties (and zero votes) resolve randomly among the
	// leading options. Pure over the tally map + option count; fixtures.
	static int WinnerFromVotes(map<string, int> votesByPlayer, int optionCount)
	{
		if (optionCount <= 0) return 0;

		array<int> tally = new array<int>;
		for (int optIdx = 0; optIdx < optionCount; optIdx++)
		{
			tally.Insert(0);
		}
		for (int voteIdx = 0; voteIdx < votesByPlayer.Count(); voteIdx++)
		{
			int votedOption = votesByPlayer.GetElement(voteIdx);
			if (votedOption >= 0 && votedOption < optionCount)
			{
				tally[votedOption] = tally[votedOption] + 1;
			}
		}

		int bestVotes = -1;
		for (int scanIdx = 0; scanIdx < optionCount; scanIdx++)
		{
			if (tally[scanIdx] > bestVotes) bestVotes = tally[scanIdx];
		}

		array<int> leaders = new array<int>;
		for (int leadIdx = 0; leadIdx < optionCount; leadIdx++)
		{
			if (tally[leadIdx] == bestVotes) leaders.Insert(leadIdx);
		}
		return leaders[Math.RandomInt(0, leaders.Count())];
	}

	static void SelfTest()
	{
		map<string, int> votes = new map<string, int>;
		votes.Set("a", 1);
		votes.Set("b", 1);
		votes.Set("c", 0);
		int winOk = 1;
		if (DmVoteService.WinnerFromVotes(votes, 3) != 1) winOk = 0;
		Print("[DM] fixture DmVoteService majority win: expected=1 got=" + winOk.ToString() + " " + DmFixture.Verdict(winOk == 1));

		map<string, int> noVotes = new map<string, int>;
		int emptyOk = 1;
		int emptyWinner = DmVoteService.WinnerFromVotes(noVotes, 4);
		if (emptyWinner < 0 || emptyWinner > 3) emptyOk = 0;
		if (DmVoteService.WinnerFromVotes(noVotes, 0) != 0) emptyOk = 0;
		Print("[DM] fixture DmVoteService empty tally: expected=1 got=" + emptyOk.ToString() + " " + DmFixture.Verdict(emptyOk == 1));

		map<string, int> oobVotes = new map<string, int>;
		oobVotes.Set("x", 99);
		oobVotes.Set("y", 2);
		int oobOk = 1;
		if (DmVoteService.WinnerFromVotes(oobVotes, 3) != 2) oobOk = 0;
		Print("[DM] fixture DmVoteService ignores out-of-bounds: expected=1 got=" + oobOk.ToString() + " " + DmFixture.Verdict(oobOk == 1));
	}
}
