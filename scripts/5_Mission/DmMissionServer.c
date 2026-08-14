// THE single `modded class MissionServer` block for this mod (same
// one-chain-link rule as capture/DmPlayerHook.c).
//
// Deliberately does NOT override OnUpdate: this mod does no per-frame work.
modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();

		if (!GetGame() || !GetGame().IsDedicatedServer()) return;

		Print("[DM] Sentinel Deathmatch v" + DmVersion.VERSION + " initializing");
		Print("[DM] rpc id range " + DmRpc.BASE.ToString() + " .. " + DmRpc.RANGE_END.ToString());

		DmRunSelfTests();

		DmConfig cfg = DmConfig.GetInstance();
		if (!cfg.IsEnabled())
		{
			Print("[DM] disabled via config.json - round engine not started");
			return;
		}

		DmRoundEngine.GetInstance().Start();

#ifdef SentinelEnforcer
		// Sentinel platform feed: compiled in only where the enforcer mod is
		// loaded (its CfgMods name is a script define). See DmSentinelConfig.c.
		SentinelAddonRegistry.Register("deathmatch", DmVersion.VERSION);
		DmSentinelBridge.GetInstance().Init();
#endif
	}

	override void InvokeOnConnect(PlayerBase player, PlayerIdentity identity)
	{
		super.InvokeOnConnect(player, identity);
		if (DmConfig.GetInstance().IsEnabled())
		{
			DmRoundEngine.GetInstance().OnPlayerJoined(identity);
		}
	}

	override void PlayerDisconnected(PlayerBase player, PlayerIdentity identity, string uid)
	{
		if (DmConfig.GetInstance().IsEnabled())
		{
			DmRoundEngine.GetInstance().OnPlayerLeft(identity);
		}
		super.PlayerDisconnected(player, identity, uid);
	}

	// Join path: new connections spawn into the deathmatch loop (at the
	// active zone when a round is armed) with the active loadout, replacing
	// the vanilla random-position + menu-equipment flow.
	override PlayerBase OnClientNewEvent(PlayerIdentity identity, vector pos, ParamsReadContext ctx)
	{
		if (!DmConfig.GetInstance().IsEnabled())
		{
			return super.OnClientNewEvent(identity, pos, ctx);
		}

		float joinYaw;
		vector joinPos = DmSpawnService.GetInstance().PickSpawnPosition(joinYaw);

		PlayerBase joined = CreateCharacter(identity, joinPos, ctx, GetGame().CreateRandomPlayer());
		if (!joined)
		{
			return super.OnClientNewEvent(identity, pos, ctx);
		}
		joined.SetOrientation(Vector(joinYaw, 0, 0));
		DmLoadoutFactory.GetInstance().Apply(joined, DmVoteService.GetInstance().GetActivePresetIndex());

		// Late state sync so the new client's HUD shows the current phase
		// (delayed - the client's mission is still initializing right now).
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DmNetServer.GetInstance().SendStateSyncTo, 3000, false, joined);
		return joined;
	}
}

// Boot-time fixtures. Every subsystem registers its SelfTest here; output is
// grep-parseable from the server script log:
//   [DM] fixture <name>: expected=<x> got=<y> PASS|FAIL
void DmRunSelfTests()
{
	Print("[DM] running boot fixtures");
	DmPhase.SelfTest();
	DmRpc.SelfTest();
	DmConfig.SelfTest();
	DmZonesConfig.SelfTest();
	DmPresetsConfig.SelfTest();
	DmApi.SelfTest();
	DmZoneService.SelfTest();
	DmSpawnService.SelfTest();
	DmLoadoutFactory.SelfTest();
	DmVoteService.SelfTest();
	DmScoreService.SelfTest();
	DmCleanupService.SelfTest();
	DmNetServer.SelfTest();
	DmClientState.SelfTest();
	DmSentinelConfig.SelfTest();
	DmRoundEngine.SelfTest();
	DmZoneMarkers.SelfTest();
	DmArenaService.SelfTest();
	Print("[DM] boot fixtures complete");
}
