// THE single `modded class MissionGameplay` block for this mod (client-side
// mission; same one-chain-link rule as the server hooks).
modded class MissionGameplay
{
	private ref DmHudController m_DmHud;

	override void OnInit()
	{
		super.OnInit();

		if (!GetGame() || GetGame().IsDedicatedServer()) return;

		m_DmHud = new DmHudController();
		m_DmHud.Init();
	}

	override void OnKeyPress(int key)
	{
		super.OnKeyPress(key);

		if (!m_DmHud) return;

		// Fixed keys for v1; migrate to inputs.xml rebindable actions later.
		if (key == KeyCode.KC_B)
		{
			m_DmHud.ToggleVoteMenu();
		}
		else if (key == KeyCode.KC_P)
		{
			m_DmHud.ToggleScoreboard();
		}
	}
}
