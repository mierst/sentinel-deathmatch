// Vote tally for zone + preset selection.
//
// Options come from the validated zone/preset lists, casts arrive via the
// VOTE_CAST RPC (rate-limited, bounds-checked, ignored outside VOTING), and
// resolution picks the highest tally with random tie-break.
//
// Two config switches shape a vote window (all read once at OpenVote, never
// per tick):
//  - ArenaSelection / PresetSelection "random": that column is not voted on
//    at all - casts are ignored, the option list on the wire is empty, and
//    Resolve rolls it uniformly (an empty tally is exactly WinnerFromVotes'
//    random-among-leaders case).
//  - AllowRandomChoice: each voted column carries one extra "Random" option
//    at index == option count. If it wins, that column is rolled.
class DmVoteService
{
	private static ref DmVoteService s_Instance;

	private bool m_Open = false;
	private bool m_ZoneVoteOpen = true;
	private bool m_PresetVoteOpen = true;
	private bool m_AllowRandom = true;
	private ref map<string, int> m_ZoneVoteByPlayer = new map<string, int>;
	private ref map<string, int> m_PresetVoteByPlayer = new map<string, int>;
	private ref map<string, float> m_LastCastAt = new map<string, float>;
	private ref map<string, bool> m_MapVoteCalls = new map<string, bool>;

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
	bool IsZoneVoteOpen() { return m_ZoneVoteOpen; }
	bool IsPresetVoteOpen() { return m_PresetVoteOpen; }
	bool IsRandomChoiceAllowed() { return m_AllowRandom; }

	// zoneVote / presetVote false = that column is rolled, not voted
	// (ArenaSelection / PresetSelection "random"). allowRandom adds the
	// "Random" pick to every voted column (AllowRandomChoice).
	void OpenVote(bool zoneVote, bool presetVote, bool allowRandom)
	{
		m_Open = true;
		m_ZoneVoteOpen = zoneVote;
		m_PresetVoteOpen = presetVote;
		m_AllowRandom = allowRandom;
		m_ZoneVoteByPlayer.Clear();
		m_PresetVoteByPlayer.Clear();
		m_LastCastAt.Clear();
		m_MapVoteCalls.Clear();
	}

	// /mapvote tally (accumulates during LIVE, cleared when the next vote
	// opens). Returns true only for a player's FIRST call.
	bool RegisterMapVoteCall(string playerId)
	{
		if (playerId == "") return false;
		bool already;
		if (m_MapVoteCalls.Find(playerId, already)) return false;
		m_MapVoteCalls.Set(playerId, true);
		return true;
	}

	int GetMapVoteCallCount() { return m_MapVoteCalls.Count(); }

	// Two thirds of the lobby, rounded up, never below 1. Pure; fixtures.
	static int MapVoteNeeded(int playerCount)
	{
		if (playerCount < 1) return 1;
		return (playerCount * 2 + 2) / 3;
	}

	// A cast is valid for a real option, or for the "Random" slot at
	// index == optionCount when that pick is allowed. Pure; fixtures.
	static bool IsCastInRange(int idx, int optionCount, bool allowRandom)
	{
		if (idx < 0) return false;
		if (idx < optionCount) return true;
		if (allowRandom && idx == optionCount) return true;
		return false;
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

		// A closed column ignores casts: a client cannot steer a rolled pick.
		if (m_ZoneVoteOpen && IsCastInRange(zoneIdx, DmZonesConfig.GetInstance().GetEnabledCount(), m_AllowRandom))
		{
			m_ZoneVoteByPlayer.Set(playerId, zoneIdx);
		}
		if (m_PresetVoteOpen && IsCastInRange(presetIdx, DmLoadoutFactory.GetInstance().GetValidPresetCount(), m_AllowRandom))
		{
			m_PresetVoteByPlayer.Set(playerId, presetIdx);
		}
	}

	// Close the vote and arm the winning zone + preset. Empty tally or tie ->
	// random among leaders, so rotation never stalls; a closed column's
	// tally is always empty, which is how "random" mode rolls it.
	void Resolve()
	{
		m_Open = false;

		int zoneCount = DmZonesConfig.GetInstance().GetEnabledCount();
		int presetCount = DmLoadoutFactory.GetInstance().GetValidPresetCount();

		m_ActiveZoneEnabledIdx = WinnerFromVotes(m_ZoneVoteByPlayer, zoneCount, m_AllowRandom);
		m_ActivePresetValidIdx = WinnerFromVotes(m_PresetVoteByPlayer, presetCount, m_AllowRandom);

		DmZoneService.GetInstance().SetActiveZone(DmZonesConfig.GetInstance().GetEnabledZone(m_ActiveZoneEnabledIdx));

		int votesCast = m_ZoneVoteByPlayer.Count();
		if (m_PresetVoteByPlayer.Count() > votesCast) votesCast = m_PresetVoteByPlayer.Count();
		DmApi.OnVoteResult().Invoke(DmZoneService.GetInstance().GetActiveZoneName(), GetActivePresetName(), votesCast);
		DmNetServer.GetInstance().SendVoteResultAll(DmZoneService.GetInstance().GetActiveZoneName(), GetActivePresetName(), votesCast);
	}

	// True once a strict majority of the connected players has voted for the
	// SAME combo across the voted columns. Drives the round engine's vote
	// fast-forward. With both columns rolled there is nothing to vote on, so
	// consensus is immediate and the window clamps to VoteConsensusSeconds.
	bool HasComboMajority(int playerCount)
	{
		if (!m_Open || playerCount <= 0) return false;
		if (!m_ZoneVoteOpen && !m_PresetVoteOpen) return true;
		return ComboMajorityCount(m_ZoneVoteByPlayer, m_PresetVoteByPlayer, m_ZoneVoteOpen, m_PresetVoteOpen) * 2 > playerCount;
	}

	// Largest bloc of players whose votes agree across the required columns.
	// With both required, players who voted only one of the two join no
	// bloc; with one required the other map is ignored; with none, 0.
	// Pure; fixtures.
	static int ComboMajorityCount(map<string, int> zoneVotesByPlayer, map<string, int> presetVotesByPlayer, bool zoneRequired, bool presetRequired)
	{
		if (!zoneRequired && !presetRequired) return 0;
		map<string, int> leadVotes = zoneVotesByPlayer;
		if (!zoneRequired) leadVotes = presetVotesByPlayer;

		map<string, int> comboTally = new map<string, int>;
		int best = 0;
		for (int voteIdx = 0; voteIdx < leadVotes.Count(); voteIdx++)
		{
			string playerId = leadVotes.GetKey(voteIdx);
			string comboKey = leadVotes.GetElement(voteIdx).ToString();
			if (zoneRequired && presetRequired)
			{
				int presetVote;
				if (!presetVotesByPlayer.Find(playerId, presetVote)) continue;
				comboKey = comboKey + "|" + presetVote.ToString();
			}
			int comboCount;
			if (!comboTally.Find(comboKey, comboCount)) comboCount = 0;
			comboCount = comboCount + 1;
			comboTally.Set(comboKey, comboCount);
			if (comboCount > best) best = comboCount;
		}
		return best;
	}

	string GetActivePresetName()
	{
		DmPresetData preset = DmLoadoutFactory.GetInstance().GetValidPreset(m_ActivePresetValidIdx);
		if (!preset) return "";
		return preset.Name;
	}

	// Highest tally wins; ties (and zero votes) resolve randomly among the
	// leading options. With allowRandom the extra slot at index optionCount
	// is the "Random" pick: if it wins, roll a real option. Pure over the
	// tally map + option count; fixtures.
	static int WinnerFromVotes(map<string, int> votesByPlayer, int optionCount, bool allowRandom)
	{
		if (optionCount <= 0) return 0;

		int slotCount = optionCount;
		if (allowRandom) slotCount = optionCount + 1;

		array<int> tally = new array<int>;
		for (int optIdx = 0; optIdx < slotCount; optIdx++)
		{
			tally.Insert(0);
		}
		for (int voteIdx = 0; voteIdx < votesByPlayer.Count(); voteIdx++)
		{
			int votedOption = votesByPlayer.GetElement(voteIdx);
			if (votedOption >= 0 && votedOption < slotCount)
			{
				tally[votedOption] = tally[votedOption] + 1;
			}
		}

		int bestVotes = -1;
		for (int scanIdx = 0; scanIdx < slotCount; scanIdx++)
		{
			if (tally[scanIdx] > bestVotes) bestVotes = tally[scanIdx];
		}

		array<int> leaders = new array<int>;
		for (int leadIdx = 0; leadIdx < slotCount; leadIdx++)
		{
			if (tally[leadIdx] == bestVotes) leaders.Insert(leadIdx);
		}
		int winner = leaders[Math.RandomInt(0, leaders.Count())];
		if (winner >= optionCount) winner = Math.RandomInt(0, optionCount);
		return winner;
	}

	static void SelfTest()
	{
		map<string, int> votes = new map<string, int>;
		votes.Set("a", 1);
		votes.Set("b", 1);
		votes.Set("c", 0);
		int winOk = 1;
		if (DmVoteService.WinnerFromVotes(votes, 3, false) != 1) winOk = 0;
		if (DmVoteService.WinnerFromVotes(votes, 3, true) != 1) winOk = 0;
		Print("[DM] fixture DmVoteService majority win: expected=1 got=" + winOk.ToString() + " " + DmFixture.Verdict(winOk == 1));

		map<string, int> noVotes = new map<string, int>;
		int emptyOk = 1;
		int emptyWinner = DmVoteService.WinnerFromVotes(noVotes, 4, false);
		if (emptyWinner < 0 || emptyWinner > 3) emptyOk = 0;
		int emptyRandWinner = DmVoteService.WinnerFromVotes(noVotes, 4, true);
		if (emptyRandWinner < 0 || emptyRandWinner > 3) emptyOk = 0;
		if (DmVoteService.WinnerFromVotes(noVotes, 0, true) != 0) emptyOk = 0;
		Print("[DM] fixture DmVoteService empty tally: expected=1 got=" + emptyOk.ToString() + " " + DmFixture.Verdict(emptyOk == 1));

		map<string, int> oobVotes = new map<string, int>;
		oobVotes.Set("x", 99);
		oobVotes.Set("y", 2);
		int oobOk = 1;
		if (DmVoteService.WinnerFromVotes(oobVotes, 3, false) != 2) oobOk = 0;
		if (DmVoteService.WinnerFromVotes(oobVotes, 3, true) != 2) oobOk = 0;
		Print("[DM] fixture DmVoteService ignores out-of-bounds: expected=1 got=" + oobOk.ToString() + " " + DmFixture.Verdict(oobOk == 1));

		// The Random pick: index == optionCount only counts when allowed. A
		// winning Random slot resolves to a real option; without the switch
		// that index is out of bounds and the real vote wins.
		map<string, int> randVotes = new map<string, int>;
		randVotes.Set("a", 3);
		randVotes.Set("b", 3);
		randVotes.Set("c", 1);
		int randOk = 1;
		int randWinner = DmVoteService.WinnerFromVotes(randVotes, 3, true);
		if (randWinner < 0 || randWinner > 2) randOk = 0;
		if (DmVoteService.WinnerFromVotes(randVotes, 3, false) != 1) randOk = 0;
		if (!DmVoteService.IsCastInRange(3, 3, true)) randOk = 0;
		if (DmVoteService.IsCastInRange(3, 3, false)) randOk = 0;
		if (!DmVoteService.IsCastInRange(2, 3, false)) randOk = 0;
		if (DmVoteService.IsCastInRange(4, 3, true)) randOk = 0;
		if (DmVoteService.IsCastInRange(-1, 3, true)) randOk = 0;
		Print("[DM] fixture DmVoteService random pick: expected=1 got=" + randOk.ToString() + " " + DmFixture.Verdict(randOk == 1));

		// Combo blocs: a+b agree on (1,0); c matches zone but not preset;
		// d voted zone only and joins no bloc.
		map<string, int> comboZones = new map<string, int>;
		map<string, int> comboPresets = new map<string, int>;
		comboZones.Set("a", 1);
		comboZones.Set("b", 1);
		comboZones.Set("c", 1);
		comboZones.Set("d", 0);
		comboPresets.Set("a", 0);
		comboPresets.Set("b", 0);
		comboPresets.Set("c", 2);
		int comboOk = 1;
		if (DmVoteService.ComboMajorityCount(comboZones, comboPresets, true, true) != 2) comboOk = 0;
		map<string, int> comboEmpty = new map<string, int>;
		if (DmVoteService.ComboMajorityCount(comboEmpty, comboPresets, true, true) != 0) comboOk = 0;
		Print("[DM] fixture DmVoteService combo bloc count: expected=1 got=" + comboOk.ToString() + " " + DmFixture.Verdict(comboOk == 1));

		// One column rolled: the bloc is by the other column alone. Zone-only
		// gives a+b+c (all zone 1); preset-only gives a+b (preset 0); both
		// rolled gives 0 from the pure function, but the service reports
		// instant consensus because there is nothing to vote on.
		int singleOk = 1;
		if (DmVoteService.ComboMajorityCount(comboZones, comboPresets, true, false) != 3) singleOk = 0;
		if (DmVoteService.ComboMajorityCount(comboZones, comboEmpty, true, false) != 3) singleOk = 0;
		if (DmVoteService.ComboMajorityCount(comboZones, comboPresets, false, true) != 2) singleOk = 0;
		if (DmVoteService.ComboMajorityCount(comboEmpty, comboPresets, false, true) != 2) singleOk = 0;
		if (DmVoteService.ComboMajorityCount(comboZones, comboPresets, false, false) != 0) singleOk = 0;
		if (DmVoteService.ComboMajorityCount(comboEmpty, comboEmpty, true, false) != 0) singleOk = 0;
		DmVoteService modeProbe = new DmVoteService();
		modeProbe.OpenVote(true, false, true);
		if (modeProbe.IsZoneVoteOpen() == false) singleOk = 0;
		if (modeProbe.IsPresetVoteOpen()) singleOk = 0;
		if (!modeProbe.IsRandomChoiceAllowed()) singleOk = 0;
		if (modeProbe.HasComboMajority(1)) singleOk = 0; // nobody voted yet
		modeProbe.OpenVote(false, false, false);
		if (!modeProbe.HasComboMajority(1)) singleOk = 0; // nothing to vote on
		if (modeProbe.IsRandomChoiceAllowed()) singleOk = 0;
		modeProbe.OpenVote(true, true, true);
		if (!modeProbe.IsZoneVoteOpen()) singleOk = 0;
		if (!modeProbe.IsPresetVoteOpen()) singleOk = 0;
		Print("[DM] fixture DmVoteService single-column bloc: expected=1 got=" + singleOk.ToString() + " " + DmFixture.Verdict(singleOk == 1));

		// /mapvote: two-thirds rounded up, dedup per player, cleared on open.
		int mapVoteOk = 1;
		if (DmVoteService.MapVoteNeeded(1) != 1) mapVoteOk = 0;
		if (DmVoteService.MapVoteNeeded(2) != 2) mapVoteOk = 0;
		if (DmVoteService.MapVoteNeeded(3) != 2) mapVoteOk = 0;
		if (DmVoteService.MapVoteNeeded(5) != 4) mapVoteOk = 0;
		if (DmVoteService.MapVoteNeeded(6) != 4) mapVoteOk = 0;
		if (DmVoteService.MapVoteNeeded(0) != 1) mapVoteOk = 0;
		DmVoteService mvProbe = new DmVoteService();
		if (!mvProbe.RegisterMapVoteCall("a")) mapVoteOk = 0;
		if (mvProbe.RegisterMapVoteCall("a")) mapVoteOk = 0;
		if (mvProbe.RegisterMapVoteCall("")) mapVoteOk = 0;
		if (mvProbe.GetMapVoteCallCount() != 1) mapVoteOk = 0;
		mvProbe.OpenVote(true, true, true);
		if (mvProbe.GetMapVoteCallCount() != 0) mapVoteOk = 0;
		Print("[DM] fixture DmVoteService mapvote threshold: expected=1 got=" + mapVoteOk.ToString() + " " + DmFixture.Verdict(mapVoteOk == 1));
	}
}
