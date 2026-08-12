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

	override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
	{
		// Cheapest gate first: everything outside our claimed id range takes
		// one int compare and falls through to vanilla.
		if (rpc_type >= DmRpc.BASE && rpc_type <= DmRpc.RANGE_END)
		{
			if (GetGame() && GetGame().IsDedicatedServer())
			{
				DmHandleServerRpc(sender, rpc_type, ctx);
			}
			return;
		}

		super.OnRPC(sender, rpc_type, ctx);
	}
}

// Server-side RPC routing. All client input is untrusted: bounds and phase
// checks live in the services, rate limiting in DmVoteService.
void DmHandleServerRpc(PlayerIdentity sender, int rpcType, ParamsReadContext ctx)
{
	if (rpcType == DmRpc.VOTE_CAST)
	{
		Param2<int, int> voteData;
		if (!ctx.Read(voteData)) return;
		DmVoteService.GetInstance().OnVoteCast(sender, voteData.param1, voteData.param2);
	}
}
