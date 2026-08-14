// RPC id block. These ride the vanilla OnRPC(int) dispatch, which is ONE
// global integer space shared with every other mod on the server - hence the
// high magic-number base. The claimed range is printed at boot so a collision
// with another mod is diagnosable from the server log.
//
// All payloads are Params of ints/floats/short strings. No JSON on the wire.
// Server -> client traffic is event-driven only (phase changes, vote windows);
// the client HUD counts down to a synced deadline rather than receiving ticks.
class DmRpc
{
	static const int BASE        = 0x44D00000;
	static const int STATE_SYNC  = 0x44D00001; // server->client: phase, deadline, scores
	static const int VOTE_OPEN   = 0x44D00002; // server->client: option lists
	static const int VOTE_CAST   = 0x44D00003; // client->server: (weaponIdx, zoneIdx)
	static const int VOTE_RESULT = 0x44D00004; // server->client: winners + tallies
	static const int SCOREBOARD  = 0x44D00005; // server->client: end-of-round rows
	static const int HUD_EVENT   = 0x44D00006; // server->client: killfeed line, zone warning
	static const int MAP_VOTE    = 0x44D00007; // client->server: /mapvote chat command
	static const int RANGE_END   = 0x44D0000F;

	static void SelfTest()
	{
		int rangeOk = 1;
		if (DmRpc.STATE_SYNC <= DmRpc.BASE) rangeOk = 0;
		if (DmRpc.HUD_EVENT >= DmRpc.RANGE_END) rangeOk = 0;
		Print("[DM] fixture DmRpc range: expected=1 got=" + rangeOk.ToString() + " " + DmFixture.Verdict(rangeOk == 1));
	}
}
