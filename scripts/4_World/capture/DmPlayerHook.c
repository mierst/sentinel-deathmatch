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
			else
			{
				DmHandleClientRpc(rpc_type, ctx);
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
	if (rpcType == DmRpc.MAP_VOTE)
	{
		DmRoundEngine.GetInstance().OnMapVoteCall(sender);
	}
}

// Client-side RPC routing: decode and hand to the state store. The UI layer
// (5_Mission) polls the store's sequence counters - no upward module calls.
void DmHandleClientRpc(int rpcType, ParamsReadContext ctx)
{
	DmClientState state = DmClientState.GetInstance();

	if (rpcType == DmRpc.STATE_SYNC)
	{
		Param9<int, int, float, string, string, float, float, float, float> syncData;
		if (!ctx.Read(syncData)) return;
		state.ApplyStateSync(syncData.param1, syncData.param2, syncData.param3, syncData.param4, syncData.param5, syncData.param6, syncData.param7, syncData.param8, syncData.param9);
		return;
	}
	if (rpcType == DmRpc.VOTE_OPEN)
	{
		Param3<float, string, string> voteOpenData;
		if (!ctx.Read(voteOpenData)) return;
		state.ApplyVoteOpen(voteOpenData.param1, voteOpenData.param2, voteOpenData.param3);
		return;
	}
	if (rpcType == DmRpc.VOTE_RESULT)
	{
		Param3<string, string, int> voteResultData;
		if (!ctx.Read(voteResultData)) return;
		state.ApplyVoteResult(voteResultData.param1, voteResultData.param2, voteResultData.param3);
		return;
	}
	if (rpcType == DmRpc.SCOREBOARD)
	{
		Param3<string, string, string> scoreboardData;
		if (!ctx.Read(scoreboardData)) return;
		state.ApplyScoreboard(scoreboardData.param1, scoreboardData.param2, scoreboardData.param3);
		return;
	}
	if (rpcType == DmRpc.HUD_EVENT)
	{
		Param2<int, string> hudData;
		if (!ctx.Read(hudData)) return;
		if (hudData.param1 == 0)
		{
			state.ApplyKillfeed(hudData.param2);
		}
		if (hudData.param1 == 1)
		{
			state.ApplyZoneCountdown(hudData.param2.ToInt());
		}
		return;
	}
}
