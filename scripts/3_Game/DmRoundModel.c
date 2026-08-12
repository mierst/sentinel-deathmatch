// Round lifecycle phases. Shared by server (round engine) and, later, the
// client UI (phase arrives via STATE_SYNC RPC).
//
// IDLE -> VOTING -> COUNTDOWN -> LIVE -> ROUNDEND -> VOTING -> ...
class DmPhase
{
	static const int IDLE      = 0;
	static const int VOTING    = 1;
	static const int COUNTDOWN = 2;
	static const int LIVE      = 3;
	static const int ROUNDEND  = 4;

	// Natural successor in the cycle. ROUNDEND loops back to VOTING, not
	// IDLE: IDLE is only re-entered when population drops below MinPlayers
	// (the engine checks that separately). if/else instead of switch: class
	// constants as case labels are not reliably accepted by the in-engine
	// compiler even when the packer is happy.
	static int Next(int phase)
	{
		if (phase == DmPhase.IDLE) return DmPhase.VOTING;
		if (phase == DmPhase.VOTING) return DmPhase.COUNTDOWN;
		if (phase == DmPhase.COUNTDOWN) return DmPhase.LIVE;
		if (phase == DmPhase.LIVE) return DmPhase.ROUNDEND;
		if (phase == DmPhase.ROUNDEND) return DmPhase.VOTING;
		return DmPhase.IDLE;
	}

	static string Name(int phase)
	{
		if (phase == DmPhase.IDLE) return "IDLE";
		if (phase == DmPhase.VOTING) return "VOTING";
		if (phase == DmPhase.COUNTDOWN) return "COUNTDOWN";
		if (phase == DmPhase.LIVE) return "LIVE";
		if (phase == DmPhase.ROUNDEND) return "ROUNDEND";
		return "UNKNOWN";
	}

	static void SelfTest()
	{
		int chainOk = 1;
		if (DmPhase.Next(DmPhase.IDLE) != DmPhase.VOTING) chainOk = 0;
		if (DmPhase.Next(DmPhase.VOTING) != DmPhase.COUNTDOWN) chainOk = 0;
		if (DmPhase.Next(DmPhase.COUNTDOWN) != DmPhase.LIVE) chainOk = 0;
		if (DmPhase.Next(DmPhase.LIVE) != DmPhase.ROUNDEND) chainOk = 0;
		if (DmPhase.Next(DmPhase.ROUNDEND) != DmPhase.VOTING) chainOk = 0;
		Print("[DM] fixture DmPhase.Next chain: expected=1 got=" + chainOk.ToString() + " " + DmFixture.Verdict(chainOk == 1));

		int nameOk = 1;
		if (DmPhase.Name(DmPhase.LIVE) != "LIVE") nameOk = 0;
		if (DmPhase.Name(99) != "UNKNOWN") nameOk = 0;
		Print("[DM] fixture DmPhase.Name: expected=1 got=" + nameOk.ToString() + " " + DmFixture.Verdict(nameOk == 1));
	}
}

// Shared fixture verdict helper so every fixture line greps identically.
class DmFixture
{
	static string Verdict(bool pass)
	{
		if (pass) return "PASS";
		return "FAIL";
	}
}
