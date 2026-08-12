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
	DmApi.SelfTest();
	DmRoundEngine.SelfTest();
	Print("[DM] boot fixtures complete");
}
