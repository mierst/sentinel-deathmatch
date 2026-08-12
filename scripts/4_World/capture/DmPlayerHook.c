// THE single `modded class PlayerBase` block for this mod.
//
// Rule: every PlayerBase override this mod ever needs lives HERE, in this one
// consolidated block. Each separate `modded class PlayerBase` declaration adds
// another link to the engine's dispatch chain for EVERY PlayerBase event on
// EVERY player; consolidating keeps the mod's chain cost at exactly one link.
// Other files contribute free functions that this block calls.
modded class PlayerBase
{
	override void EEKilled(Object killer)
	{
		super.EEKilled(killer);

		if (!GetGame() || !GetGame().IsDedicatedServer()) return;

		DmRoundEngine.GetInstance().OnPlayerKilled(this, killer);
	}
}
