// Sentinel platform integration, compiled into the core mod CONDITIONALLY.
//
// The emitters below are wrapped in #ifdef SentinelEnforcer: every loaded
// mod's CfgMods class name becomes a script-compile define, so this code
// exists only where the Sentinel Enforcer is actually loaded - it compiles
// out on clients (the enforcer is -serverMod-only and never ships to them)
// and on servers that don't run the platform. No requiredAddons anywhere, so
// the Windows "requires addon" blocking dialog can never fire; a separate
// bridge PBO was the earlier design and was collapsed for exactly that
// reason. This block is also the reference example of consuming the
// enforcer's emission surface from a third-party mod.
//
// Broadcasting is optional at three stacked levels, all operator-controlled:
//   1. The enforcer's own master enable (disabled or credential-less
//      enforcer means nothing ever emits).
//   2. This config: $profile:SentinelDeathmatch\sentinel.json
//      (EventFeed kills the whole dm.* feed; GlobalLeaderboard governs
//      ranked participation and is read by the platform, not scripts).
//   3. The enforcer's per-event-type capture flags (ShouldCapture).
//
// The config class itself compiles UNCONDITIONALLY (it references no
// enforcer types), so sentinel.json semantics stay stable either way.
class DmSentinelConfigData
{
	bool EventFeed = true;
	bool GlobalLeaderboard = true;
}

class DmSentinelConfig
{
	static string CONFIG_PATH = "$profile:SentinelDeathmatch\\sentinel.json";

	private static ref DmSentinelConfig s_Instance;

	private ref DmSentinelConfigData m_Data;

	static DmSentinelConfig GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new DmSentinelConfig();
			s_Instance.Load();
		}
		return s_Instance;
	}

	void Load()
	{
		m_Data = new DmSentinelConfigData();
		if (FileExist(CONFIG_PATH))
		{
			JsonFileLoader<DmSentinelConfigData>.JsonLoadFile(CONFIG_PATH, m_Data);
		}
		else
		{
			JsonFileLoader<DmSentinelConfigData>.JsonSaveFile(CONFIG_PATH, m_Data);
		}
	}

	bool IsEventFeedEnabled() { return m_Data.EventFeed; }
	bool IsGlobalLeaderboardEnabled() { return m_Data.GlobalLeaderboard; }

	static void SelfTest()
	{
		DmSentinelConfigData defaults = new DmSentinelConfigData();
		int defOk = 1;
		if (!defaults.EventFeed) defOk = 0;
		if (!defaults.GlobalLeaderboard) defOk = 0;
		Print("[DM] fixture DmSentinelConfig defaults: expected=1 got=" + defOk.ToString() + " " + DmFixture.Verdict(defOk == 1));
	}
}

#ifdef SentinelEnforcer
class DmSentinelBridge
{
	private static ref DmSentinelBridge s_Instance;

	private bool m_Subscribed = false;

	static DmSentinelBridge GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new DmSentinelBridge();
		}
		return s_Instance;
	}

	void Init()
	{
		if (m_Subscribed) return;
		m_Subscribed = true;

		if (!DmSentinelConfig.GetInstance().IsEventFeedEnabled())
		{
			Print("[DM-Sentinel] EventFeed disabled in sentinel.json - dm.* feed off");
			return;
		}

		DmApi.OnRoundStart().Insert(EmitRoundStart);
		DmApi.OnRoundEnd().Insert(EmitRoundEnd);
		DmApi.OnKill().Insert(EmitKill);
		DmApi.OnVoteResult().Insert(EmitVoteResult);
		Print("[DM-Sentinel] bridge subscribed to DmApi (dm.* feed on)");
	}

	// Canonical emit shape: cheapest gates first, allocate last.
	void EmitRoundStart(int roundId, string zoneName, string presetName, int playerCount)
	{
		if (!SentinelConfig.GetInstance().IsEnabled()) return;
		if (!SentinelEventQueue.ShouldCapture("dm.round_start")) return;

		SentinelEvent e = new SentinelEvent("dm.round_start");
		e.SetInt("round_id", roundId);
		e.SetString("zone", zoneName);
		e.SetString("preset", presetName);
		e.SetInt("player_count", playerCount);
		e.SetBool("ranked", DmSentinelConfig.GetInstance().IsGlobalLeaderboardEnabled());
		SentinelEventQueue.GetInstance().Enqueue(e, false);
	}

	void EmitRoundEnd(int roundId, string zoneName, string presetName, int durationSeconds, string winnerId)
	{
		if (!SentinelConfig.GetInstance().IsEnabled()) return;
		if (!SentinelEventQueue.ShouldCapture("dm.round_end")) return;

		SentinelEvent e = new SentinelEvent("dm.round_end");
		e.m_PlayerId = winnerId;
		e.SetInt("round_id", roundId);
		e.SetString("zone", zoneName);
		e.SetString("preset", presetName);
		e.SetInt("duration_s", durationSeconds);
		e.SetBool("ranked", DmSentinelConfig.GetInstance().IsGlobalLeaderboardEnabled());
		SentinelEventQueue.GetInstance().Enqueue(e, false);
	}

	void EmitKill(string killerId, string victimId, string weapon, float distance, bool headshot, int killerStreak)
	{
		if (!SentinelConfig.GetInstance().IsEnabled()) return;
		if (!SentinelEventQueue.ShouldCapture("dm.kill")) return;

		SentinelEvent e = new SentinelEvent("dm.kill");
		e.m_PlayerId = killerId;
		e.m_TargetPlayerId = victimId;
		e.SetInt("round_id", DmApi.GetRoundId());
		e.SetString("weapon", weapon);
		e.SetFloat("distance", distance);
		e.SetBool("headshot", headshot);
		e.SetInt("streak", killerStreak);
		SentinelEventQueue.GetInstance().Enqueue(e, false);
	}

	void EmitVoteResult(string zoneName, string presetName, int votesCast)
	{
		if (!SentinelConfig.GetInstance().IsEnabled()) return;
		if (!SentinelEventQueue.ShouldCapture("dm.vote_result")) return;

		SentinelEvent e = new SentinelEvent("dm.vote_result");
		e.SetString("zone", zoneName);
		e.SetString("preset", presetName);
		e.SetInt("votes_cast", votesCast);
		SentinelEventQueue.GetInstance().Enqueue(e, false);
	}
}
#endif
