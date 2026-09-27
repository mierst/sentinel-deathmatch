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

	// Request-driven bounded leaderboard. Internal request/revision state keeps
	// late packets from replacing a newer tab or page.
	ref array<ref DmLeaderboardRow> m_LeaderboardRows = new array<ref DmLeaderboardRow>;
	ref DmLeaderboardRow m_LeaderboardSelf;
	int m_LeaderboardTotal = 0;
	int m_LeaderboardOffset = 0;
	int m_LeaderboardSeq = 0;
	bool m_LeaderboardSession = false;
	string m_LeaderboardError = "";
	ref DmLeaderboardTheme m_LeaderboardTheme = new DmLeaderboardTheme();
	int m_LeaderboardThemeSeq = 0;
	private int m_LeaderboardNextRequestId = 0;
	private int m_LeaderboardPendingRequestId = 0;
	private int m_LeaderboardRoundRevision = -1;
	private int m_LeaderboardSessionRevision = -1;
	private bool m_LeaderboardPending = false;
	private bool m_LeaderboardQueued = false;
	private bool m_LeaderboardWantedSession = false;
	private int m_LeaderboardWantedOffset = 0;
	private bool m_LeaderboardWantedFindSelf = false;
	private float m_LeaderboardRequestAt = -1000;
	private float m_LeaderboardLastPollAt = -1000;

	// Killfeed ring (newest first)
	ref array<string> m_KillfeedLines = new array<string>;
	ref array<float> m_KillfeedTimes = new array<float>;
	int m_KillfeedSeq = 0;

	// CLIENT_OPTS (server-decided switches; 0 until the join RPC lands, so
	// every switch reads as off before the server has spoken)
	int m_ClientOpts = 0;

	void ApplyClientOptions(int mask)
	{
		m_ClientOpts = mask;
	}

	bool IsChatHistoryOnOpen()
	{
		return DmClientOpts.Has(m_ClientOpts, DmClientOpts.CHAT_HISTORY_ON_OPEN);
	}

	bool IsRandomChoiceAllowed()
	{
		return DmClientOpts.Has(m_ClientOpts, DmClientOpts.ALLOW_RANDOM_CHOICE);
	}

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

	void ApplyRoundEndNotice(string winnerName)
	{
		m_WinnerName = DmLeaderboard.CleanField(winnerName);
		m_ScoreboardSeq = m_ScoreboardSeq + 1;
	}

	void ApplyLeaderboardTheme(DmLeaderboardTheme theme)
	{
		if (!theme)
		{
			m_LeaderboardTheme = new DmLeaderboardTheme();
			m_LeaderboardThemeSeq = m_LeaderboardThemeSeq + 1;
			return;
		}
		theme.Validate();
		m_LeaderboardTheme = theme;
		m_LeaderboardThemeSeq = m_LeaderboardThemeSeq + 1;
	}

	static bool CanApplyLeaderboardResponse(int pendingRequestId, int acceptedRevision, int responseRequestId, int responseRevision)
	{
		if (pendingRequestId <= 0) return false;
		if (responseRequestId != pendingRequestId) return false;
		if (responseRevision < acceptedRevision) return false;
		return true;
	}

	private void FailLeaderboardResponse(string errorText)
	{
		m_LeaderboardPending = false;
		m_LeaderboardError = errorText;
		m_LeaderboardSeq = m_LeaderboardSeq + 1;
	}

	void ApplyLeaderboardPage(int protocolVersion, int requestId, int revision, int sessionInt, int offset, int total, array<string> encodedRows, string selfBlob, string errorText)
	{
		if (!m_LeaderboardPending) return;
		if (requestId != m_LeaderboardPendingRequestId) return;
		if (m_LeaderboardQueued)
		{
			m_LeaderboardPending = false;
			return;
		}
		if (protocolVersion != DmLeaderboard.PROTOCOL_VERSION)
		{
			FailLeaderboardResponse("Leaderboard protocol mismatch");
			return;
		}
		int acceptedRevision = m_LeaderboardRoundRevision;
		if (sessionInt == 1) acceptedRevision = m_LeaderboardSessionRevision;
		if (!DmClientState.CanApplyLeaderboardResponse(m_LeaderboardPendingRequestId, acceptedRevision, requestId, revision))
		{
			FailLeaderboardResponse("Stale leaderboard response");
			return;
		}
		m_LeaderboardPending = false;
		if (sessionInt != 0 && sessionInt != 1)
		{
			FailLeaderboardResponse("Invalid leaderboard response");
			return;
		}
		if (offset < 0 || total < 0)
		{
			FailLeaderboardResponse("Invalid leaderboard bounds");
			return;
		}

		array<ref DmLeaderboardRow> decodedRows = new array<ref DmLeaderboardRow>;
		bool rowsValid = DmLeaderboard.DecodeRowList(encodedRows, decodedRows);
		if (!rowsValid || decodedRows.Count() > DmLeaderboard.MAX_ROWS)
		{
			FailLeaderboardResponse("Leaderboard page too large");
			return;
		}
		DmLeaderboardRow decodedSelf = DmLeaderboard.DecodeRow(selfBlob);
		m_LeaderboardRows = decodedRows;
		m_LeaderboardSelf = decodedSelf;
		m_LeaderboardTotal = total;
		m_LeaderboardOffset = DmLeaderboard.ClampOffset(offset, total);
		m_LeaderboardSession = sessionInt == 1;
		m_LeaderboardError = DmLeaderboard.CleanField(errorText);
		if (!m_LeaderboardQueued)
		{
			m_LeaderboardWantedSession = m_LeaderboardSession;
			m_LeaderboardWantedOffset = m_LeaderboardOffset;
			m_LeaderboardWantedFindSelf = false;
		}
		if (m_LeaderboardSession) m_LeaderboardSessionRevision = revision;
		else m_LeaderboardRoundRevision = revision;
		m_LeaderboardSeq = m_LeaderboardSeq + 1;
	}

	private void SendLeaderboardRequestNow(bool session, int boundedOffset, bool findSelf, float requestNow)
	{
		bool requestTimedOut = m_LeaderboardPending && requestNow - m_LeaderboardRequestAt >= DmLeaderboard.CLIENT_TIMEOUT_SECONDS;
		if (requestTimedOut)
		{
			m_LeaderboardError = "Leaderboard request timed out; retrying";
			m_LeaderboardSeq = m_LeaderboardSeq + 1;
		}
		else
		{
			m_LeaderboardError = "";
		}
		PlayerBase requestPlayer = PlayerBase.Cast(GetGame().GetPlayer());
		if (!requestPlayer) return;
		m_LeaderboardQueued = false;
		m_LeaderboardNextRequestId = m_LeaderboardNextRequestId + 1;
		if (m_LeaderboardNextRequestId <= 0) m_LeaderboardNextRequestId = 1;
		int sessionInt = 0;
		if (session) sessionInt = 1;
		int findSelfInt = 0;
		if (findSelf) findSelfInt = 1;
		m_LeaderboardPendingRequestId = m_LeaderboardNextRequestId;
		m_LeaderboardPending = true;
		m_LeaderboardWantedSession = session;
		m_LeaderboardWantedOffset = boundedOffset;
		m_LeaderboardWantedFindSelf = findSelf;
		m_LeaderboardRequestAt = requestNow;
		m_LeaderboardLastPollAt = requestNow;
		GetGame().RPCSingleParam(requestPlayer, DmRpc.LEADERBOARD_REQUEST, new Param5<int, int, int, int, int>(DmLeaderboard.PROTOCOL_VERSION, m_LeaderboardPendingRequestId, sessionInt, boundedOffset, findSelfInt), true);
	}

	void RequestLeaderboard(bool session, int offset, bool findSelf = false)
	{
		if (!GetGame()) return;
		float requestNow = GetGame().GetTickTime();
		int boundedOffset = offset;
		if (boundedOffset < 0) boundedOffset = 0;
		if (boundedOffset > DmLeaderboard.MAX_OFFSET) boundedOffset = DmLeaderboard.MAX_OFFSET;
		if (m_LeaderboardPending && requestNow - m_LeaderboardRequestAt < DmLeaderboard.CLIENT_TIMEOUT_SECONDS && m_LeaderboardWantedSession == session && m_LeaderboardWantedOffset == boundedOffset && m_LeaderboardWantedFindSelf == findSelf) return;
		m_LeaderboardWantedSession = session;
		m_LeaderboardWantedOffset = boundedOffset;
		m_LeaderboardWantedFindSelf = findSelf;
		if (requestNow - m_LeaderboardRequestAt < DmLeaderboard.CLIENT_SEND_SECONDS)
		{
			m_LeaderboardQueued = true;
			m_LeaderboardError = "";
			return;
		}
		SendLeaderboardRequestNow(session, boundedOffset, findSelf, requestNow);
	}

	void PollLeaderboard()
	{
		if (!GetGame()) return;
		float pollNow = GetGame().GetTickTime();
		if (m_LeaderboardQueued)
		{
			if (pollNow - m_LeaderboardRequestAt < DmLeaderboard.CLIENT_SEND_SECONDS) return;
			SendLeaderboardRequestNow(m_LeaderboardWantedSession, m_LeaderboardWantedOffset, m_LeaderboardWantedFindSelf, pollNow);
			return;
		}
		if (pollNow - m_LeaderboardLastPollAt < DmLeaderboard.CLIENT_POLL_SECONDS) return;
		m_LeaderboardLastPollAt = pollNow;
		if (m_LeaderboardPending && pollNow - m_LeaderboardRequestAt < DmLeaderboard.CLIENT_TIMEOUT_SECONDS) return;
		RequestLeaderboard(m_LeaderboardWantedSession, m_LeaderboardWantedOffset, m_LeaderboardWantedFindSelf);
	}

	// Zone grace countdown (HUD_EVENT type 1). Freshness-gated on read: the
	// server only sends while the player is outside, so a stale value just
	// ages out rather than needing an explicit clear.
	int m_ZoneCountdownSeconds = 0;
	float m_ZoneCountdownAt = 0;

	void ApplyZoneCountdown(int secondsLeft)
	{
		m_ZoneCountdownSeconds = secondsLeft;
		m_ZoneCountdownAt = GetGame().GetTickTime();
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

		int staleOk = 1;
		if (!DmClientState.CanApplyLeaderboardResponse(8, 4, 8, 4)) staleOk = 0;
		if (DmClientState.CanApplyLeaderboardResponse(8, 4, 7, 5)) staleOk = 0;
		if (DmClientState.CanApplyLeaderboardResponse(8, 4, 8, 3)) staleOk = 0;
		if (DmClientState.CanApplyLeaderboardResponse(0, 0, 0, 0)) staleOk = 0;
		Print("[DM] fixture DmClientState stale leaderboard response: expected=1 got=" + staleOk.ToString() + " " + DmFixture.Verdict(staleOk == 1));
	}
}
