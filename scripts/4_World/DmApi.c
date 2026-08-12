// Public observer surface. Third-party server-side mods (stats exporters,
// killfeed relays, telemetry integrations) subscribe here; the deathmatch mod
// itself carries no knowledge of any consumer.
//
// Stability contract: invoker signatures and getter names are stable across
// minor versions. Payloads are plain types only - no consumer-specific types
// may ever appear in this file.
//
// Invoker signatures:
//   OnRoundStart(int roundId, string zoneName, string presetName, int playerCount)
//   OnRoundEnd(int roundId, string zoneName, string presetName, int durationSeconds, string winnerId)
//   OnKill(string killerId, string victimId, string weapon, float distance, bool headshot, int killerStreak)
//   OnVoteResult(string zoneName, string presetName, int votesCast)
class DmApi
{
	private static ref ScriptInvoker s_OnRoundStart;
	private static ref ScriptInvoker s_OnRoundEnd;
	private static ref ScriptInvoker s_OnKill;
	private static ref ScriptInvoker s_OnVoteResult;

	static ScriptInvoker OnRoundStart()
	{
		if (!s_OnRoundStart) s_OnRoundStart = new ScriptInvoker();
		return s_OnRoundStart;
	}

	static ScriptInvoker OnRoundEnd()
	{
		if (!s_OnRoundEnd) s_OnRoundEnd = new ScriptInvoker();
		return s_OnRoundEnd;
	}

	static ScriptInvoker OnKill()
	{
		if (!s_OnKill) s_OnKill = new ScriptInvoker();
		return s_OnKill;
	}

	static ScriptInvoker OnVoteResult()
	{
		if (!s_OnVoteResult) s_OnVoteResult = new ScriptInvoker();
		return s_OnVoteResult;
	}

	// Pull-side getters for consumers that need enrichment context.
	static int GetRoundId()
	{
		return DmRoundEngine.GetInstance().GetRoundId();
	}

	static int GetPhase()
	{
		return DmRoundEngine.GetInstance().GetPhase();
	}

	static string GetModVersion()
	{
		return DmVersion.VERSION;
	}

	static string GetActiveZoneName()
	{
		return DmZoneService.GetInstance().GetActiveZoneName();
	}

	static string GetActivePresetName()
	{
		return DmVoteService.GetInstance().GetActivePresetName();
	}

	static void SelfTest()
	{
		int invokerOk = 1;
		if (!DmApi.OnRoundStart()) invokerOk = 0;
		if (!DmApi.OnKill()) invokerOk = 0;
		Print("[DM] fixture DmApi invokers: expected=1 got=" + invokerOk.ToString() + " " + DmFixture.Verdict(invokerOk == 1));
	}
}
