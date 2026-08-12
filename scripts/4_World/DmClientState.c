// Client-side state store, written by the RPC handlers (PlayerBase.OnRPC
// client branch) and read by the 5_Mission UI layer.
//
// Module rule: 4_World cannot reference 5_Mission UI classes, so this store
// never calls the UI. Instead each update bumps a sequence counter; the HUD
// controller's 500 ms tick notices changed sequences and reacts. Data flows
// up, never sideways.
class DmClientState
{
	private static ref DmClientState s_Instance;

	// STATE_SYNC
	int m_Phase = 0;
	int m_RoundId = 0;
	float m_PhaseEndTime = 0; // local engine-time deadline
	string m_ZoneName = "";
	string m_PresetName = "";
	float m_ZoneCX = 0;
	float m_ZoneCZ = 0;
	float m_ZoneRadius = 0;
	float m_ZoneWarnMargin = 0;
	int m_StateSeq = 0;

	// VOTE_OPEN / VOTE_RESULT
	ref array<string> m_ZoneOptions = new array<string>;
	ref array<string> m_PresetOptions = new array<string>;
	float m_VoteEndTime = 0;
	int m_VoteSeq = 0;
	string m_VoteResultText = "";
	int m_VoteResultSeq = 0;

	// SCOREBOARD
	string m_ScoreboardBlob = "";
	string m_SessionBlob = "";
	string m_WinnerName = "";
	int m_ScoreboardSeq = 0;

	// Killfeed ring (newest first)
	ref array<string> m_KillfeedLines = new array<string>;
	ref array<float> m_KillfeedTimes = new array<float>;
	int m_KillfeedSeq = 0;

	static DmClientState GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new DmClientState();
		}
		return s_Instance;
	}

	void ApplyStateSync(int phase, int roundId, float remainSeconds, string zoneName, string presetName, float cx, float cz, float radius, float warnMargin)
	{
		m_Phase = phase;
		m_RoundId = roundId;
		m_PhaseEndTime = GetGame().GetTickTime() + remainSeconds;
		m_ZoneName = zoneName;
		m_PresetName = presetName;
		m_ZoneCX = cx;
		m_ZoneCZ = cz;
		m_ZoneRadius = radius;
		m_ZoneWarnMargin = warnMargin;
		m_StateSeq = m_StateSeq + 1;
	}

	void ApplyVoteOpen(float voteSeconds, string zoneBlob, string presetBlob)
	{
		SplitBlob(zoneBlob, m_ZoneOptions);
		SplitBlob(presetBlob, m_PresetOptions);
		m_VoteEndTime = GetGame().GetTickTime() + voteSeconds;
		m_VoteSeq = m_VoteSeq + 1;
	}

	void ApplyVoteResult(string zoneName, string presetName, int votesCast)
	{
		m_VoteResultText = "Next: " + zoneName + " / " + presetName + " (" + votesCast.ToString() + " votes)";
		m_VoteResultSeq = m_VoteResultSeq + 1;
	}

	void ApplyScoreboard(string rowsBlob, string sessionRowsBlob, string winnerName)
	{
		m_ScoreboardBlob = rowsBlob;
		m_SessionBlob = sessionRowsBlob;
		m_WinnerName = winnerName;
		m_ScoreboardSeq = m_ScoreboardSeq + 1;
	}

	void ApplyKillfeed(string line)
	{
		m_KillfeedLines.InsertAt(line, 0);
		m_KillfeedTimes.InsertAt(GetGame().GetTickTime(), 0);
		while (m_KillfeedLines.Count() > 5)
		{
			m_KillfeedLines.Remove(m_KillfeedLines.Count() - 1);
			m_KillfeedTimes.Remove(m_KillfeedTimes.Count() - 1);
		}
		m_KillfeedSeq = m_KillfeedSeq + 1;
	}

	float GetPhaseRemaining()
	{
		float remain = m_PhaseEndTime - GetGame().GetTickTime();
		if (remain < 0) remain = 0;
		return remain;
	}

	// Meters outside the active zone boundary for a position; negative inside.
	float OwnOvershoot(vector pos)
	{
		if (m_ZoneRadius <= 0) return -999999;
		return DmZoneService.OutDistance2D(pos[0], pos[2], m_ZoneCX, m_ZoneCZ, m_ZoneRadius);
	}

	static void SplitBlob(string blob, array<string> outLines)
	{
		outLines.Clear();
		if (blob == "") return;
		blob.Split("\n", outLines);
	}

	static void SelfTest()
	{
		array<string> parts = new array<string>;
		DmClientState.SplitBlob("Alpha\nBravo\nCharlie", parts);
		int splitOk = 1;
		if (parts.Count() != 3) splitOk = 0;
		if (parts.Count() == 3 && parts[1] != "Bravo") splitOk = 0;
		DmClientState.SplitBlob("", parts);
		if (parts.Count() != 0) splitOk = 0;
		Print("[DM] fixture DmClientState blob split: expected=1 got=" + splitOk.ToString() + " " + DmFixture.Verdict(splitOk == 1));
	}
}
