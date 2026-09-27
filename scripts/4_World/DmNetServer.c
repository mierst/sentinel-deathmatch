// Server -> client sends. Event-driven only: state syncs on phase change and
// join, vote windows at open/close, scoreboard at round end, killfeed lines
// per kill. Nothing periodic - the client HUD counts down to a synced
// deadline locally. (The one scheduled send, rotating announcements, is a
// plain chat line paced in minutes by DmAnnounceService off the 500 ms tick.)
//
// Sends are targeted per player object (the proven scripted-RPC reception
// path is the target object's OnRPC on the receiving side).
class DmNetServer
{
	private static ref DmNetServer s_Instance;

	private ref array<Man> m_SendScratch = new array<Man>;
	private ref map<string, float> m_LastLeaderboardRequestAt = new map<string, float>;
	private int m_VoteWindowId = 0;
	private ref array<string> m_VoteZones = new array<string>;
	private ref array<string> m_VotePresets = new array<string>;

	static DmNetServer GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new DmNetServer();
		}
		return s_Instance;
	}

	private void SendParamToAll(int rpcId, Param payload)
	{
		m_SendScratch.Clear();
		GetGame().GetPlayers(m_SendScratch);
		for (int sendIdx = 0; sendIdx < m_SendScratch.Count(); sendIdx++)
		{
			Man target = m_SendScratch[sendIdx];
			PlayerIdentity ident = target.GetIdentity();
			if (!ident) continue;
			GetGame().RPCSingleParam(target, rpcId, payload, true, ident);
		}
	}

	private Param BuildStateSyncParam()
	{
		DmRoundEngine engine = DmRoundEngine.GetInstance();
		float remainSeconds = engine.GetPhaseDeadline() - GetGame().GetTickTime();
		if (remainSeconds < 0) remainSeconds = 0;

		string zoneName = DmZoneService.GetInstance().GetActiveZoneName();
		string presetName = DmVoteService.GetInstance().GetActivePresetName();
		float zoneCX = 0;
		float zoneCZ = 0;
		float zoneRadius = 0;
		float zoneWarnMargin = 0;
		DmZoneData zone = DmZoneService.GetInstance().GetActiveZone();
		if (zone)
		{
			zoneCX = zone.CenterX;
			zoneCZ = zone.CenterZ;
			zoneRadius = zone.Radius;
			zoneWarnMargin = zone.WarnMargin;
		}

		return new Param9<int, int, float, string, string, float, float, float, float>(
			engine.GetPhase(), engine.GetRoundId(), remainSeconds, zoneName, presetName, zoneCX, zoneCZ, zoneRadius, zoneWarnMargin);
	}

	void SendStateSyncAll()
	{
		SendParamToAll(DmRpc.STATE_SYNC, BuildStateSyncParam());
	}

	// Join-time client option switches (config-driven, server-decided).
	void SendClientOptionsTo(PlayerBase pb)
	{
		if (!pb) return;
		PlayerIdentity optIdent = pb.GetIdentity();
		if (!optIdent) return;
		DmConfig optCfg = DmConfig.GetInstance();
		int mask = DmClientOpts.Pack(optCfg.IsChatHistoryOnOpenEnabled(), optCfg.IsRandomChoiceAllowed());
		GetGame().RPCSingleParam(pb, DmRpc.CLIENT_OPTS, new Param1<int>(mask), true, optIdent);
		DmLeaderboardTheme joinTheme = DmLeaderboardThemeStore.Get();
		GetGame().RPCSingleParam(pb, DmRpc.LEADERBOARD_THEME, new Param1<ref DmLeaderboardTheme>(joinTheme), true, optIdent);
		GetGame().RPCSingleParam(pb, DmRpc.VOTE_THEME, new Param1<ref DmLeaderboardTheme>(DmLeaderboardThemeStore.GetVote()), true, optIdent);
	}

	void SendStateSyncTo(PlayerBase pb)
	{
		if (!pb) return;
		SendClientOptionsTo(pb);
		PlayerIdentity ident = pb.GetIdentity();
		if (!ident) return;
		GetGame().RPCSingleParam(pb, DmRpc.STATE_SYNC, BuildStateSyncParam(), true, ident);
		if (DmRoundEngine.GetInstance().GetPhase() == DmPhase.VOTING) SendVoteOptionsTo(pb);
		if (DmRoundEngine.GetInstance().GetPhase() == DmPhase.ROUNDEND)
		{
			GetGame().RPCSingleParam(pb, DmRpc.ROUND_END_NOTICE, new Param1<string>(DmLeaderboard.CleanField(DmScoreService.GetInstance().LeaderName())), true, ident);
		}
	}

	// A window snapshot is immutable until the next vote. Array positions are
	// the original vote indices, including for labels flattened for display.
	void SendVoteOpenAll(float voteSeconds)
	{
		m_VoteWindowId = m_VoteWindowId + 1;
		m_VoteZones.Clear();
		m_VotePresets.Clear();
		if (DmVoteService.GetInstance().IsZoneVoteOpen())
		{
			DmZonesConfig zones = DmZonesConfig.GetInstance();
			for (int zoneIdx = 0; zoneIdx < zones.GetEnabledCount(); zoneIdx++)
			{
				m_VoteZones.Insert(DmVoteOptions.Label(zones.GetEnabledZone(zoneIdx).Name));
			}
		}
		if (DmVoteService.GetInstance().IsPresetVoteOpen())
		{
			DmLoadoutFactory loadouts = DmLoadoutFactory.GetInstance();
			for (int presetIdx = 0; presetIdx < loadouts.GetValidPresetCount(); presetIdx++)
			{
				m_VotePresets.Insert(DmVoteOptions.Label(loadouts.GetValidPreset(presetIdx).Name));
			}
		}
		SendVoteOptionsTo(null);
	}

	private void SendVoteOptionsTo(PlayerBase target)
	{
		if (m_VoteWindowId <= 0) return;
		PlayerIdentity voteIdentity;
		if (target)
		{
			voteIdentity = target.GetIdentity();
			if (!voteIdentity) return;
		}
		int maxCount = m_VoteZones.Count();
		if (m_VotePresets.Count() > maxCount) maxCount = m_VotePresets.Count();
		int chunkCount = 1;
		if (maxCount > 0) chunkCount = ((maxCount - 1) / DmVoteOptions.CHUNK_SIZE) + 1;
		for (int chunkIdx = 0; chunkIdx < chunkCount; chunkIdx++)
		{
			int offset = chunkIdx * DmVoteOptions.CHUNK_SIZE;
			float remain = DmRoundEngine.GetInstance().GetPhaseDeadline() - GetGame().GetTickTime();
			if (remain < 0) remain = 0;
			ref array<string> zoneChunk = DmVoteOptions.Chunk(m_VoteZones, offset);
			ref array<string> presetChunk = DmVoteOptions.Chunk(m_VotePresets, offset);
			Param7<int, float, int, int, int, array<string>, array<string>> payload = new Param7<int, float, int, int, int, array<string>, array<string>>(m_VoteWindowId, remain, m_VoteZones.Count(), m_VotePresets.Count(), offset, zoneChunk, presetChunk);
			if (target) GetGame().RPCSingleParam(target, DmRpc.VOTE_OPTIONS, payload, true, voteIdentity);
			else SendParamToAll(DmRpc.VOTE_OPTIONS, payload);
		}
	}
	void SendVoteResultAll(string zoneName, string presetName, int votesCast)
	{
		SendParamToAll(DmRpc.VOTE_RESULT, new Param3<string, string, int>(zoneName, presetName, votesCast));
	}

	void SendScoreboardAll(string rowsBlob, string sessionRowsBlob, string winnerName)
	{
		SendParamToAll(DmRpc.SCOREBOARD, new Param3<string, string, string>(rowsBlob, sessionRowsBlob, winnerName));
	}

	void SendRoundEndNoticeAll(string winnerName)
	{
		SendParamToAll(DmRpc.ROUND_END_NOTICE, new Param1<string>(DmLeaderboard.CleanField(winnerName)));
	}

	static bool CanServeLeaderboardRequest(float lastRequestAt, float nowSeconds)
	{
		if (lastRequestAt < 0) return true;
		return nowSeconds - lastRequestAt >= DmLeaderboard.SERVER_RATE_SECONDS;
	}

	private PlayerBase FindPlayerForIdentity(PlayerIdentity ident)
	{
		if (!ident) return null;
		string identityId = ident.GetPlainId();
		if (identityId == "") return null;
		m_SendScratch.Clear();
		GetGame().GetPlayers(m_SendScratch);
		for (int identityFindIdx = 0; identityFindIdx < m_SendScratch.Count(); identityFindIdx++)
		{
			PlayerBase identityTarget = PlayerBase.Cast(m_SendScratch[identityFindIdx]);
			if (!identityTarget) continue;
			PlayerIdentity candidateIdentity = identityTarget.GetIdentity();
			if (candidateIdentity && candidateIdentity.GetPlainId() == identityId) return identityTarget;
		}
		return null;
	}

	private void SendLeaderboardProtocolError(PlayerIdentity sender, int requestId, int sessionInt)
	{
		if (!sender || requestId <= 0) return;
		PlayerBase errorTarget = FindPlayerForIdentity(sender);
		if (!errorTarget) return;
		int errorSession = 0;
		if (sessionInt == 1) errorSession = 1;
		ref array<string> errorRows = new array<string>;
		Param9<int, int, int, int, int, int, array<string>, string, string> errorPayload = new Param9<int, int, int, int, int, int, array<string>, string, string>(DmLeaderboard.PROTOCOL_VERSION, requestId, 0, errorSession, 0, 0, errorRows, "", "Leaderboard protocol mismatch");
		GetGame().RPCSingleParam(errorTarget, DmRpc.LEADERBOARD_PAGE, errorPayload, true, sender);
	}

	void HandleLeaderboardRequest(PlayerIdentity sender, int protocolVersion, int requestId, int sessionInt, int requestedOffset, int findSelfInt)
	{
		bool requestDebug = DmConfig.GetInstance().IsDebug();
		if (requestDebug) Print("[DM] leaderboard request handler entered");
		if (!sender || requestId <= 0)
		{
			if (requestDebug) Print("[DM] leaderboard request rejected: sender/request");
			return;
		}
		if (protocolVersion != DmLeaderboard.PROTOCOL_VERSION)
		{
			if (requestDebug) Print("[DM] leaderboard request rejected: protocol");
			SendLeaderboardProtocolError(sender, requestId, sessionInt);
			return;
		}
		if (sessionInt != 0 && sessionInt != 1)
		{
			if (requestDebug) Print("[DM] leaderboard request rejected: session flag");
			return;
		}
		if (findSelfInt != 0 && findSelfInt != 1)
		{
			if (requestDebug) Print("[DM] leaderboard request rejected: self flag");
			return;
		}
		string senderId = sender.GetPlainId();
		if (senderId == "")
		{
			if (requestDebug) Print("[DM] leaderboard request rejected: empty sender");
			return;
		}
		float requestNow = GetGame().GetTickTime();
		float previousRequestAt = -1;
		m_LastLeaderboardRequestAt.Find(senderId, previousRequestAt);
		if (!DmNetServer.CanServeLeaderboardRequest(previousRequestAt, requestNow))
		{
			if (requestDebug) Print("[DM] leaderboard request rate limited");
			return;
		}
		m_LastLeaderboardRequestAt.Set(senderId, requestNow);

		PlayerBase requestTarget = FindPlayerForIdentity(sender);
		if (!requestTarget)
		{
			if (requestDebug) Print("[DM] leaderboard request rejected: target lookup");
			return;
		}
		bool session = sessionInt == 1;
		bool findSelf = findSelfInt == 1;
		DmLeaderboardPage responsePage = DmScoreService.GetInstance().BuildLeaderboardPage(session, senderId, requestedOffset, findSelf);
		ref array<string> responseRows = new array<string>;
		DmLeaderboard.EncodeRowList(responsePage.Rows, responseRows);
		string responseSelf = DmLeaderboard.EncodeRow(responsePage.SelfRow);
		Param9<int, int, int, int, int, int, array<string>, string, string> responsePayload = new Param9<int, int, int, int, int, int, array<string>, string, string>(DmLeaderboard.PROTOCOL_VERSION, requestId, responsePage.Revision, sessionInt, responsePage.Offset, responsePage.Total, responseRows, responseSelf, "");
		GetGame().RPCSingleParam(requestTarget, DmRpc.LEADERBOARD_PAGE, responsePayload, true, sender);
		if (requestDebug) Print("[DM] leaderboard page response sent");
	}

	// Direct chat line to one player (command feedback etc.).
	void SendChatTo(PlayerIdentity ident, string text)
	{
		if (!ident) return;
		m_SendScratch.Clear();
		GetGame().GetPlayers(m_SendScratch);
		for (int findIdx = 0; findIdx < m_SendScratch.Count(); findIdx++)
		{
			Man target = m_SendScratch[findIdx];
			if (target.GetIdentity() == ident)
			{
				GetGame().ChatMP(target, text, "colorAction");
				return;
			}
		}
	}

	// Zone grace countdown, targeted at the player who is outside. Rides
	// HUD_EVENT as type 1; type 0 stays the killfeed.
	void SendZoneCountdownTo(PlayerBase pb, int secondsLeft)
	{
		if (!pb) return;
		PlayerIdentity ident = pb.GetIdentity();
		if (!ident) return;
		GetGame().RPCSingleParam(pb, DmRpc.HUD_EVENT, new Param2<int, string>(1, secondsLeft.ToString()), true, ident);
	}

	void SendKillfeedAll(string line)
	{
		SendParamToAll(DmRpc.HUD_EVENT, new Param2<int, string>(0, line));

		// Chat copy: unlike the 8 s HUD rows, chat scrollback survives the
		// victim's own death/respawn blackout.
		if (!DmConfig.GetInstance().IsKillfeedToChatEnabled()) return;
		SendChatAll(line, "colorAction");
	}

	// Chat line to every connected player. colorClass is a vanilla ChatLine
	// style name (colorAction / colorImportant / colorFriendly /
	// colorStatusChannel); DmConfig validates the announcement one at load.
	void SendChatAll(string text, string colorClass)
	{
		m_SendScratch.Clear();
		GetGame().GetPlayers(m_SendScratch);
		for (int chatIdx = 0; chatIdx < m_SendScratch.Count(); chatIdx++)
		{
			Man chatTarget = m_SendScratch[chatIdx];
			if (!chatTarget.GetIdentity()) continue;
			GetGame().ChatMP(chatTarget, text, colorClass);
		}
	}

	// Pure formatter so the fixture can cover it.
	static string FormatKillfeedLine(string killerName, string victimName, string weapon, float distance)
	{
		int distMeters = distance;
		string line = killerName;
		if (weapon != "")
		{
			line = line + " [" + weapon + " " + distMeters.ToString() + "m]";
		}
		line = line + " > " + victimName;
		return line;
	}

	static void SelfTest()
	{
		int fmtOk = 1;
		string withWeapon = DmNetServer.FormatKillfeedLine("Alice", "Bob", "MP5K", 42.7);
		if (withWeapon != "Alice [MP5K 42m] > Bob") fmtOk = 0;
		string bareHands = DmNetServer.FormatKillfeedLine("Alice", "Bob", "", 3.0);
		if (bareHands != "Alice > Bob") fmtOk = 0;
		Print("[DM] fixture DmNetServer killfeed format: expected=1 got=" + fmtOk.ToString() + " " + DmFixture.Verdict(fmtOk == 1));

		int rateOk = 1;
		if (!DmNetServer.CanServeLeaderboardRequest(-1, 10.0)) rateOk = 0;
		if (DmNetServer.CanServeLeaderboardRequest(10.0, 10.24)) rateOk = 0;
		if (!DmNetServer.CanServeLeaderboardRequest(10.0, 10.25)) rateOk = 0;
		Print("[DM] fixture DmNetServer leaderboard rate: expected=1 got=" + rateOk.ToString() + " " + DmFixture.Verdict(rateOk == 1));
	}
}
