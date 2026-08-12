// Bridge registration. Lives in 5_Mission because it modded-classes
// MissionServer (not declared yet at World-module compile time).
//
// This is a separate PBO from the core mod, so this is its own single chain
// link - the one-chain-link-per-modded-class rule applies per PBO.
modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();

		if (!GetGame() || !GetGame().IsDedicatedServer()) return;

		DmSentinelBridge.SelfTest();
		SentinelAddonRegistry.Register("deathmatch", DmVersion.VERSION);
		DmSentinelBridge.GetInstance().Init();
	}
}
