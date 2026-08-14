// Server round state machine.
//
// Performance posture: ONE repeating 500 ms Timer drives everything. There is
// no per-frame work anywhere in this mod. The common tick path is integer/
// float compares against cached deadlines; the player list is fetched once
// per tick into a reused scratch array and shared with the services.
class DmRoundEngine
{
	private static ref DmRoundEngine s_Instance;

	private ref Timer m_TickTimer;
	private int m_Phase = DmPhase.IDLE;
	private int m_RoundId = 0;
	private float m_PhaseDeadline = 0;   // engine seconds; 0 = no deadline armed
	private bool m_VoteFastForwarded = false; // consensus clamp fired this vote
	private float m_RoundStartedAt = 0;
	private bool m_Started = false;

	private ref array<Man> m_PlayerScratch = new array<Man>;
	private int m_TopUpCounter = 0;

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

		DmLoadoutFactory.GetInstance().ValidateAll();
		DmZonesConfig.GetInstance(); // load + validate zones.json
		DmCleanupService.GetInstance().Start();

		m_TickTimer = new Timer(CALL_CATEGORY_SYSTEM);
		m_TickTimer.Run(0.5, this, "OnTick", null, true);
		Print("[DM] round engine started v" + DmVersion.VERSION);
	}

	int GetPhase() { return m_Phase; }
	int GetRoundId() { return m_RoundId; }
	float GetPhaseDeadline() { return m_PhaseDeadline; }

	void OnTick()
	{
		if (!DmConfig.GetInstance().IsEnabled()) return;

		m_PlayerScratch.Clear();
		GetGame().GetPlayers(m_PlayerScratch);
		int playerCount = m_PlayerScratch.Count();
		float nowSeconds = GetGame().GetTickTime();

		// Survival pressure removal: every 20 ticks (~10 s), all phases.
		m_TopUpCounter = m_TopUpCounter + 1;
		if (m_TopUpCounter >= 20)
		{
			m_TopUpCounter = 0;
			if (DmConfig.GetInstance().IsSurvivalPressureDisabled())
			{
				TopUpSurvival();
			}
		}

		// Population gate applies in every phase except IDLE itself: losing
		// the room mid-anything drops the loop back to IDLE cleanly.
		if (m_Phase != DmPhase.IDLE && playerCount < DmConfig.GetInstance().GetMinPlayers())
		{
			Print("[DM] population below MinPlayers - returning to IDLE");
			DmScoreService.GetInstance().Reset();
			m_PhaseDeadline = 0;
			TransitionTo(DmPhase.IDLE);
			DmNetServer.GetInstance().SendStateSyncAll();
			return;
		}

		if (m_Phase == DmPhase.IDLE)
		{
			if (playerCount >= DmConfig.GetInstance().GetMinPlayers())
			{
				EnterVoting(nowSeconds);
			}
			return;
		}

		if (m_Phase == DmPhase.VOTING)
		{
			// Consensus fast-forward: a strict majority on one zone+preset
			// combo clamps the remaining window (once; never extends).
			if (!m_VoteFastForwarded && DmVoteService.GetInstance().HasComboMajority(playerCount))
			{
				m_VoteFastForwarded = true;
				float consensusDeadline = nowSeconds + DmConfig.GetInstance().GetVoteConsensusSeconds();
				if (consensusDeadline < m_PhaseDeadline)
				{
					m_PhaseDeadline = consensusDeadline;
					DmNetServer.GetInstance().SendStateSyncAll();
					Print("[DM] vote consensus reached - closing vote in " + DmConfig.GetInstance().GetVoteConsensusSeconds().ToString() + "s");
				}
			}
			if (nowSeconds >= m_PhaseDeadline)
			{
				DmVoteService.GetInstance().Resolve();
				EnterCountdown(nowSeconds);
			}
			return;
		}

		if (m_Phase == DmPhase.COUNTDOWN)
		{
			if (nowSeconds >= m_PhaseDeadline)
			{
				EnterLive(nowSeconds, playerCount);
			}
			return;
		}

		if (m_Phase == DmPhase.LIVE)
		{
			DmZoneService.GetInstance().Enforce(m_PlayerScratch, 0.5);
			DmSpawnService.GetInstance().TickProtection(m_PlayerScratch, nowSeconds);

			bool timeUp = nowSeconds >= m_PhaseDeadline;
			bool scoreHit = DmScoreService.GetInstance().LeaderScore() >= DmConfig.GetInstance().GetScoreLimit();
			if (timeUp || scoreHit)
			{
				EnterRoundEnd(nowSeconds);
			}
			return;
		}

		if (m_Phase == DmPhase.ROUNDEND)
		{
			if (nowSeconds >= m_PhaseDeadline)
			{
				EnterVoting(nowSeconds);
			}
			return;
		}
	}

	// /mapvote entry (RPC-routed). Two thirds of the lobby ends the round on
	// the spot and reopens arena+preset voting.
	void OnMapVoteCall(PlayerIdentity sender)
	{
		if (m_Phase != DmPhase.LIVE || !sender) return;
		if (!DmVoteService.GetInstance().RegisterMapVoteCall(sender.GetPlainId())) return;

		m_PlayerScratch.Clear();
		GetGame().GetPlayers(m_PlayerScratch);
		int callCount = DmVoteService.GetInstance().GetMapVoteCallCount();
		int neededCount = DmVoteService.MapVoteNeeded(m_PlayerScratch.Count());
		DmNetServer.GetInstance().SendKillfeedAll(sender.GetName() + " called a map vote (" + callCount.ToString() + "/" + neededCount.ToString() + ")");

		if (callCount >= neededCount)
		{
			Print("[DM] map vote passed (" + callCount.ToString() + "/" + neededCount.ToString() + ") - ending round");
			DmNetServer.GetInstance().SendKillfeedAll("Map vote passed - back to the lobby");
			EnterVoting(GetGame().GetTickTime());
		}
	}

	private void EnterVoting(float nowSeconds)
	{
		m_VoteFastForwarded = false;
		DmVoteService.GetInstance().OpenVote();
		m_PhaseDeadline = nowSeconds + DmConfig.GetInstance().GetVoteSeconds();
		TransitionTo(DmPhase.VOTING);
		DmNetServer.GetInstance().SendStateSyncAll();
		DmNetServer.GetInstance().SendVoteOpenAll(DmConfig.GetInstance().GetVoteSeconds());
	}

	private void EnterCountdown(float nowSeconds)
	{
		DmScoreService.GetInstance().Reset();

		// Teleport everyone to spawn points and inject the winning loadout at
		// countdown START: the countdown window doubles as the client's
		// streaming warm-up for the (possibly new) arena footprint.
		DmZoneData zone = DmZoneService.GetInstance().GetActiveZone();
		int presetIdx = DmVoteService.GetInstance().GetActivePresetIndex();
		for (int playerIdx = 0; playerIdx < m_PlayerScratch.Count(); playerIdx++)
		{
			PlayerBase pb = PlayerBase.Cast(m_PlayerScratch[playerIdx]);
			if (!pb || !pb.IsAlive()) continue;

			if (zone && zone.SpawnPoints.Count() > 0)
			{
				DmSpawnPointData sp = zone.SpawnPoints[playerIdx % zone.SpawnPoints.Count()];
				pb.SetPosition(DmSpawnService.ResolveSpawnPos(sp));
				pb.SetOrientation(Vector(sp.Yaw, 0, 0));
			}
			DmLoadoutFactory.GetInstance().Apply(pb, presetIdx);
		}

		m_PhaseDeadline = nowSeconds + DmConfig.GetInstance().GetCountdownSeconds();
		TransitionTo(DmPhase.COUNTDOWN);
		DmNetServer.GetInstance().SendStateSyncAll();
	}

	private void EnterLive(float nowSeconds, int playerCount)
	{
		m_RoundId = m_RoundId + 1;
		m_RoundStartedAt = nowSeconds;
		m_PhaseDeadline = nowSeconds + DmConfig.GetInstance().GetRoundSeconds();
		TransitionTo(DmPhase.LIVE);
		DmNetServer.GetInstance().SendStateSyncAll();

		DmApi.OnRoundStart().Invoke(m_RoundId, DmZoneService.GetInstance().GetActiveZoneName(), DmVoteService.GetInstance().GetActivePresetName(), playerCount);
	}

	private void EnterRoundEnd(float nowSeconds)
	{
		int durationSeconds = nowSeconds - m_RoundStartedAt;
		string winnerId = DmScoreService.GetInstance().LeaderId();

		DmScoreService.GetInstance().PrintSummary();
		DmCleanupService.GetInstance().ExpireAll();

		m_PhaseDeadline = nowSeconds + DmConfig.GetInstance().GetScoreboardSeconds();
		TransitionTo(DmPhase.ROUNDEND);
		DmNetServer.GetInstance().SendStateSyncAll();
		DmNetServer.GetInstance().SendScoreboardAll(DmScoreService.GetInstance().BuildRowsBlob(), DmScoreService.GetInstance().BuildSessionRowsBlob(), DmScoreService.GetInstance().LeaderName());

		DmApi.OnRoundEnd().Invoke(m_RoundId, DmZoneService.GetInstance().GetActiveZoneName(), DmVoteService.GetInstance().GetActivePresetName(), durationSeconds, winnerId);
	}

	// No thirst, hunger, cold, or exhaustion in an arena: pin the survival
	// stats at full and stamina at max. Uses the already-fetched scratch list.
	private void TopUpSurvival()
	{
		for (int topUpIdx = 0; topUpIdx < m_PlayerScratch.Count(); topUpIdx++)
		{
			PlayerBase pb = PlayerBase.Cast(m_PlayerScratch[topUpIdx]);
			if (!pb || !pb.IsAlive()) continue;

			pb.GetStatWater().Set(PlayerConstants.SL_WATER_MAX);
			pb.GetStatEnergy().Set(PlayerConstants.SL_ENERGY_MAX);
			pb.GetStatHeatComfort().Set(0);
			if (pb.GetStaminaHandler())
			{
				pb.GetStaminaHandler().SetStamina(GameConstants.STAMINA_MAX);
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

	// Called from the consolidated PlayerBase hook (capture/DmPlayerHook.c).
	void OnPlayerKilled(PlayerBase victim, Object killerSource)
	{
		if (!DmConfig.GetInstance().IsEnabled()) return;
		if (!victim) return;

		PlayerIdentity victimIdent = victim.GetIdentity();
		if (!victimIdent) return;

		// Corpse cleanup + respawn run in every phase; scoring only in LIVE.
		DmCleanupService.GetInstance().RegisterCorpse(victim);
		DmSpawnService.GetInstance().ScheduleRespawn(victimIdent);

		if (m_Phase != DmPhase.LIVE) return;

		string killerId = "";
		string killerName = "";
		string weaponName = "";
		float killDistance = 0;
		PlayerBase killerPlayer = ExtractKillerPlayer(killerSource);
		if (killerPlayer)
		{
			PlayerIdentity killerIdent = killerPlayer.GetIdentity();
			if (killerIdent)
			{
				killerId = killerIdent.GetPlainId();
				killerName = killerIdent.GetName();
			}
			killDistance = vector.Distance(killerPlayer.GetPosition(), victim.GetPosition());
			EntityAI inHands = killerPlayer.GetHumanInventory().GetEntityInHands();
			if (inHands)
			{
				weaponName = inHands.GetType();
			}
		}

		int newStreak = DmScoreService.GetInstance().RegisterKill(killerId, killerName, victimIdent.GetPlainId(), victimIdent.GetName());

		if (killerId != "" && killerId != victimIdent.GetPlainId())
		{
			DmApi.OnKill().Invoke(killerId, victimIdent.GetPlainId(), weaponName, killDistance, false, newStreak);
			DmNetServer.GetInstance().SendKillfeedAll(DmNetServer.FormatKillfeedLine(killerName, victimIdent.GetName(), weaponName, killDistance));
		}
		else
		{
			// No killer to credit: the death itself costs a kill point.
			DmScoreService.GetInstance().RegisterPenalty(victimIdent.GetPlainId(), victimIdent.GetName());
			if (DmZoneService.GetInstance().ConsumeZoneKill(victimIdent.GetPlainId()))
			{
				DmNetServer.GetInstance().SendKillfeedAll(victimIdent.GetName() + " left the zone");
			}
			else
			{
				DmNetServer.GetInstance().SendKillfeedAll(victimIdent.GetName() + " died");
			}
		}
	}

	// The killer source of EEKilled can be the weapon entity, a projectile,
	// or the player; walk to the owning player when there is one.
	static PlayerBase ExtractKillerPlayer(Object killerSource)
	{
		if (!killerSource) return null;

		PlayerBase direct = PlayerBase.Cast(killerSource);
		if (direct) return direct;

		EntityAI killerEntity = EntityAI.Cast(killerSource);
		if (!killerEntity) return null;
		return PlayerBase.Cast(killerEntity.GetHierarchyRootPlayer());
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

		int extractOk = 1;
		if (DmRoundEngine.ExtractKillerPlayer(null)) extractOk = 0;
		Print("[DM] fixture DmRoundEngine killer extract null: expected=1 got=" + extractOk.ToString() + " " + DmFixture.Verdict(extractOk == 1));
	}
}
