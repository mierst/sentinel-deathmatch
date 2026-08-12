// Server round state machine.
//
// Performance posture: ONE repeating 500 ms Timer drives everything. There is
// no per-frame work anywhere in this mod. The common tick path is integer
// compares against cached deadlines - no allocation, no string work.
class DmRoundEngine
{
	private static ref DmRoundEngine s_Instance;

	private ref Timer m_TickTimer;
	private int m_Phase = DmPhase.IDLE;
	private int m_RoundId = 0;
	private float m_PhaseDeadline = 0; // engine seconds; 0 = no deadline armed
	private bool m_Started = false;

	static DmRoundEngine GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new DmRoundEngine();
		}
		return s_Instance;
	}

	void Start()
	{
		if (m_Started) return;
		m_Started = true;

		m_TickTimer = new Timer(CALL_CATEGORY_SYSTEM);
		m_TickTimer.Run(0.5, this, "OnTick", null, true);
		Print("[DM] round engine started v" + DmVersion.VERSION);
	}

	int GetPhase() { return m_Phase; }
	int GetRoundId() { return m_RoundId; }

	void OnTick()
	{
		if (!DmConfig.GetInstance().IsEnabled()) return;

		// Phase 0 scaffold: population gate only. Vote/spawn/zone services
		// arrive in later phases; transitions past IDLE are inert until then.
		if (m_Phase == DmPhase.IDLE)
		{
			int playerCount = CountPlayers();
			if (playerCount >= DmConfig.GetInstance().GetMinPlayers())
			{
				TransitionTo(DmPhase.VOTING);
			}
		}
	}

	void TransitionTo(int nextPhase)
	{
		int prevPhase = m_Phase;
		m_Phase = nextPhase;
		if (DmConfig.GetInstance().IsDebug())
		{
			Print("[DM] phase " + DmPhase.Name(prevPhase) + " -> " + DmPhase.Name(nextPhase));
		}
	}

	private int CountPlayers()
	{
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		return players.Count();
	}

	// Called from the consolidated PlayerBase hook (capture/DmPlayerHook.c).
	void OnPlayerKilled(PlayerBase victim, Object killerSource)
	{
		if (!DmConfig.GetInstance().IsEnabled()) return;
		if (m_Phase != DmPhase.LIVE)
		{
			// Kills outside LIVE (warm-up, scoreboard) never score.
			return;
		}
		// Scoring lands in a later phase; the hook chain is proven now.
	}

	static void SelfTest()
	{
		DmRoundEngine probe = new DmRoundEngine();
		int initOk = 1;
		if (probe.GetPhase() != DmPhase.IDLE) initOk = 0;
		if (probe.GetRoundId() != 0) initOk = 0;
		Print("[DM] fixture DmRoundEngine init state: expected=1 got=" + initOk.ToString() + " " + DmFixture.Verdict(initOk == 1));

		probe.TransitionTo(DmPhase.VOTING);
		int transOk = 1;
		if (probe.GetPhase() != DmPhase.VOTING) transOk = 0;
		Print("[DM] fixture DmRoundEngine transition: expected=1 got=" + transOk.ToString() + " " + DmFixture.Verdict(transOk == 1));
	}
}
