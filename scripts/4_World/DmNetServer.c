// Server -> client sends. Event-driven only: state syncs on phase change and
// join, vote windows at open/close, scoreboard at round end, killfeed lines
// per kill. Nothing periodic - the client HUD counts down to a synced
// deadline locally.
//
// Sends are targeted per player object (the proven scripted-RPC reception
// path is the target object's OnRPC on the receiving side).
class DmNetServer
{
	private static ref DmNetServer s_Instance;

	private ref array<Man> m_SendScratch = new array<Man>;

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

	void SendStateSyncTo(PlayerBase pb)
	{
		if (!pb) return;
		PlayerIdentity ident = pb.GetIdentity();
		if (!ident) return;
		GetGame().RPCSingleParam(pb, DmRpc.STATE_SYNC, BuildStateSyncParam(), true, ident);
	}

	// Option lists ride as newline-joined blobs (names are sanitized of
	// newlines at config load by their loaders' validation).
	void SendVoteOpenAll(float voteSeconds)
	{
		string zoneBlob = "";
		DmZonesConfig zones = DmZonesConfig.GetInstance();
		for (int zoneIdx = 0; zoneIdx < zones.GetEnabledCount(); zoneIdx++)
		{
			if (zoneIdx > 0) zoneBlob = zoneBlob + "\n";
			zoneBlob = zoneBlob + zones.GetEnabledZone(zoneIdx).Name;
		}

		string presetBlob = "";
		DmLoadoutFactory loadouts = DmLoadoutFactory.GetInstance();
		for (int presetIdx = 0; presetIdx < loadouts.GetValidPresetCount(); presetIdx++)
		{
			if (presetIdx > 0) presetBlob = presetBlob + "\n";
			presetBlob = presetBlob + loadouts.GetValidPreset(presetIdx).Name;
		}

		SendParamToAll(DmRpc.VOTE_OPEN, new Param3<float, string, string>(voteSeconds, zoneBlob, presetBlob));
	}

	void SendVoteResultAll(string zoneName, string presetName, int votesCast)
	{
		SendParamToAll(DmRpc.VOTE_RESULT, new Param3<string, string, int>(zoneName, presetName, votesCast));
	}

	void SendScoreboardAll(string rowsBlob, string sessionRowsBlob, string winnerName)
	{
		SendParamToAll(DmRpc.SCOREBOARD, new Param3<string, string, string>(rowsBlob, sessionRowsBlob, winnerName));
	}

	void SendKillfeedAll(string line)
	{
		SendParamToAll(DmRpc.HUD_EVENT, new Param2<int, string>(0, line));
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
	}
}
