// Spawn point selection, server-driven respawn, spawn protection.
//
// Respawn path (no client round-trip): EEKilled -> CallLater(RespawnDelay) ->
// MissionServer.CreateCharacter directly (vanilla CreateCharacter calls
// g_Game.SelectPlayer itself) -> staged loadout injection.
class DmSpawnService
{
	private static ref DmSpawnService s_Instance;

	// Spawn-protection records: steam64 -> engine-time deadline. The primary
	// re-enable is a CallLater on the player; this map is the fail-safe swept
	// from the round engine tick so a lost deferral can never leave god mode.
	private ref map<string, float> m_ProtectUntil = new map<string, float>;

	static DmSpawnService GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new DmSpawnService();
		}
		return s_Instance;
	}

	// Pick the spawn point whose nearest enemy is farthest away. Pure static
	// over plain arrays so fixtures cover it without a live server.
	static int BestSpawnIndex(array<ref DmSpawnPointData> points, array<vector> enemyPositions)
	{
		return BestSpawnIndexAvoiding(points, enemyPositions, Vector(0, 0, 0), 0);
	}

	// Same scoring, but spawn points within avoidRadiusMeters of deathPos are
	// filtered out first - nobody respawns on top of where they just died.
	// If the filter would empty the list every point stays a candidate
	// (a tiny arena beats no spawn at all). Pure; fixtures.
	static int BestSpawnIndexAvoiding(array<ref DmSpawnPointData> points, array<vector> enemyPositions, vector deathPos, float avoidRadiusMeters)
	{
		if (!points || points.Count() == 0) return -1;

		array<int> candidates = new array<int>;
		if (avoidRadiusMeters > 0)
		{
			float avoidSq = avoidRadiusMeters * avoidRadiusMeters;
			for (int filterIdx = 0; filterIdx < points.Count(); filterIdx++)
			{
				DmSpawnPointData fsp = points[filterIdx];
				float fdx = fsp.X - deathPos[0];
				float fdz = fsp.Z - deathPos[2];
				if (fdx * fdx + fdz * fdz >= avoidSq) candidates.Insert(filterIdx);
			}
		}
		if (candidates.Count() == 0)
		{
			for (int allIdx = 0; allIdx < points.Count(); allIdx++)
			{
				candidates.Insert(allIdx);
			}
		}

		if (!enemyPositions || enemyPositions.Count() == 0)
		{
			return candidates[Math.RandomInt(0, candidates.Count())];
		}

		int bestIdx = candidates[0];
		float bestScore = -1;
		for (int candIdx = 0; candIdx < candidates.Count(); candIdx++)
		{
			DmSpawnPointData sp = points[candidates[candIdx]];
			float nearestSq = 9999999999.0;
			for (int enemyIdx = 0; enemyIdx < enemyPositions.Count(); enemyIdx++)
			{
				vector ep = enemyPositions[enemyIdx];
				float dx = sp.X - ep[0];
				float dz = sp.Z - ep[2];
				float dSq = dx * dx + dz * dz;
				if (dSq < nearestSq) nearestSq = dSq;
			}
			if (nearestSq > bestScore)
			{
				bestScore = nearestSq;
				bestIdx = candidates[candIdx];
			}
		}
		return bestIdx;
	}

	// Y=0 in zones.json means "snap to terrain".
	static vector ResolveSpawnPos(DmSpawnPointData sp)
	{
		float y = sp.Y;
		if (y == 0)
		{
			y = GetGame().SurfaceY(sp.X, sp.Z);
		}
		return Vector(sp.X, y, sp.Z);
	}

	// Choose a spawn position in the active zone, away from living enemies
	// and away from deathPos (pass "0 0 0" when there is no death, e.g. a
	// fresh join).
	vector PickSpawnPosition(out float outYaw, vector deathPos)
	{
		outYaw = 0;
		DmZoneData zone = DmZoneService.GetInstance().GetActiveZone();
		if (!zone || zone.SpawnPoints.Count() == 0)
		{
			// No zone armed (e.g. mid-IDLE join): fall back to world center
			// of the first enabled zone, else origin - loudly.
			DmZoneData fallback = DmZonesConfig.GetInstance().GetEnabledZone(0);
			if (fallback && fallback.SpawnPoints.Count() > 0)
			{
				DmSpawnPointData fsp = fallback.SpawnPoints[0];
				outYaw = fsp.Yaw;
				return ResolveSpawnPos(fsp);
			}
			Print("[DM] WARNING: no zone available for spawn - using origin");
			return Vector(0, 0, 0);
		}

		array<vector> enemyPositions = new array<vector>;
		array<Man> livePlayers = new array<Man>;
		GetGame().GetPlayers(livePlayers);
		for (int playerIdx = 0; playerIdx < livePlayers.Count(); playerIdx++)
		{
			PlayerBase pb = PlayerBase.Cast(livePlayers[playerIdx]);
			if (pb && pb.IsAlive())
			{
				enemyPositions.Insert(pb.GetPosition());
			}
		}

		int chosenIdx = BestSpawnIndexAvoiding(zone.SpawnPoints, enemyPositions, deathPos, DmConfig.GetInstance().GetRespawnAvoidDeathMeters());
		if (chosenIdx < 0) chosenIdx = 0;
		DmSpawnPointData chosen = zone.SpawnPoints[chosenIdx];
		outYaw = chosen.Yaw;
		return ResolveSpawnPos(chosen);
	}

	// ---- death memory (client-driven respawn) ----
	//
	// Respawn is executed by the ENGINE's own respawn login: with the
	// respawn dialog disabled the client auto-sends its respawn event, the
	// login flow re-enters OnClientNewEvent, and our mission override spawns
	// the new body. A parallel server-timer respawn (the original design)
	// raced that login FSM and produced duplicate bodies plus
	// "Login timed out (WaitPreloadCamRespawnState)" kicks - never again.
	// All EEKilled leaves behind is the death position, consumed by the next
	// spawn pick so the respawn still avoids the death spot.

	static const float DEATH_MEMORY_SECONDS = 60.0;

	private ref map<string, vector> m_DeathPos = new map<string, vector>;
	private ref map<string, float> m_DeathAt = new map<string, float>;

	void NoteDeath(PlayerIdentity identity, vector deathPos)
	{
		if (!identity) return;
		NoteDeathById(identity.GetPlainId(), deathPos, GetGame().GetTickTime());
	}

	void NoteDeathById(string playerId, vector deathPos, float nowSeconds)
	{
		m_DeathPos.Set(playerId, deathPos);
		m_DeathAt.Set(playerId, nowSeconds);
	}

	vector ConsumeDeathPos(PlayerIdentity identity)
	{
		if (!identity) return Vector(0, 0, 0);
		return ConsumeDeathPosById(identity.GetPlainId(), GetGame().GetTickTime());
	}

	// One-shot and freshness-gated: a stale death (a rejoin minutes later)
	// must not bias the join spawn. Pure over explicit time; fixtures.
	vector ConsumeDeathPosById(string playerId, float nowSeconds)
	{
		vector deathPos;
		if (!m_DeathPos.Find(playerId, deathPos)) return Vector(0, 0, 0);
		float diedAt;
		m_DeathAt.Find(playerId, diedAt);
		m_DeathPos.Remove(playerId);
		m_DeathAt.Remove(playerId);
		if (nowSeconds - diedAt > DEATH_MEMORY_SECONDS) return Vector(0, 0, 0);
		return deathPos;
	}

	// SetAllowDamage(false) only stops engine hit damage. Script-side health
	// writes still land on a protected player: DecreaseHealth (zone soft
	// wall, other mods), modifiers, and bleeding (vanilla skips bleed
	// creation for invincible players in DEVELOPER builds only). Players
	// took damage inside the window (field report, 09-27), so the window
	// also re-tops vitals on a short timer. Cost: SpawnProtectSeconds * 4 calls
	// per spawn, off every tick and hook path.
	static const int PROTECT_HOLD_MS = 250;

	void ApplyProtection(PlayerBase pb)
	{
		int protectSeconds = DmConfig.GetInstance().GetSpawnProtectSeconds();
		if (protectSeconds <= 0) return;

		PlayerIdentity ident = pb.GetIdentity();
		if (!ident) return;

		pb.SetAllowDamage(false);
		StartProtectionById(ident.GetPlainId(), GetGame().GetTickTime(), protectSeconds);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(EndProtection, protectSeconds * 1000, false, pb);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(HoldProtection, PROTECT_HOLD_MS, false, pb);
	}

	void StartProtectionById(string playerId, float nowSeconds, int protectSeconds)
	{
		if (protectSeconds <= 0) return;
		m_ProtectUntil.Set(playerId, nowSeconds + protectSeconds);
	}

	bool IsProtectedById(string playerId, float nowSeconds)
	{
		float until;
		if (!m_ProtectUntil.Find(playerId, until)) return false;
		return nowSeconds < until;
	}

	// Self-rescheduling until the window closes or the body is gone.
	void HoldProtection(PlayerBase pb)
	{
		if (!pb || !pb.IsAlive()) return;
		PlayerIdentity ident = pb.GetIdentity();
		if (!ident) return;
		if (!IsProtectedById(ident.GetPlainId(), GetGame().GetTickTime())) return;

		RestoreVitals(pb);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(HoldProtection, PROTECT_HOLD_MS, false, pb);
	}

	void EndProtection(PlayerBase pb)
	{
		if (!pb) return;
		pb.SetAllowDamage(true);
		// A body that died inside its window has already been replaced: the
		// record now belongs to the respawned body and must survive.
		if (!pb.IsAlive()) return;
		PlayerIdentity ident = pb.GetIdentity();
		if (ident)
		{
			m_ProtectUntil.Remove(ident.GetPlainId());
		}
	}

	// Full vitals, no bleeds, sound legs. Guarded so an already-healthy
	// player costs reads only (no synced writes).
	static void RestoreVitals(PlayerBase pb)
	{
		RestoreHealthType(pb, "Health");
		RestoreHealthType(pb, "Blood");
		RestoreHealthType(pb, "Shock");
		if (pb.GetBleedingSourceCount() > 0 && pb.GetBleedingManagerServer())
		{
			pb.GetBleedingManagerServer().RemoveAllSources();
		}
		if (pb.GetBrokenLegs() != eBrokenLegs.NO_BROKEN_LEGS)
		{
			pb.SetBrokenLegs(eBrokenLegs.NO_BROKEN_LEGS);
		}
	}

	private static void RestoreHealthType(PlayerBase pb, string healthType)
	{
		float maxValue = pb.GetMaxHealth("GlobalHealth", healthType);
		if (pb.GetHealth("GlobalHealth", healthType) < maxValue)
		{
			pb.SetHealth("GlobalHealth", healthType, maxValue);
		}
	}

	// Fail-safe sweep from the round engine tick: only scans when records
	// exist, so the common-path cost is one Count() check.
	void TickProtection(array<Man> players, float nowSeconds)
	{
		if (m_ProtectUntil.Count() == 0) return;

		for (int playerIdx = 0; playerIdx < players.Count(); playerIdx++)
		{
			PlayerBase pb = PlayerBase.Cast(players[playerIdx]);
			if (!pb) continue;
			PlayerIdentity ident = pb.GetIdentity();
			if (!ident) continue;

			float until;
			if (m_ProtectUntil.Find(ident.GetPlainId(), until))
			{
				if (nowSeconds > until + 1.0)
				{
					pb.SetAllowDamage(true);
					m_ProtectUntil.Remove(ident.GetPlainId());
					Print("[DM] WARNING: spawn-protection fail-safe fired for " + ident.GetName());
				}
			}
		}
	}

	static void SelfTest()
	{
		array<ref DmSpawnPointData> points = new array<ref DmSpawnPointData>;
		DmSpawnPointData nearPoint = new DmSpawnPointData();
		nearPoint.X = 0;
		nearPoint.Z = 0;
		points.Insert(nearPoint);
		DmSpawnPointData farPoint = new DmSpawnPointData();
		farPoint.X = 500;
		farPoint.Z = 0;
		points.Insert(farPoint);

		array<vector> enemies = new array<vector>;
		enemies.Insert(Vector(10, 0, 0)); // right on top of point 0

		int pickOk = 1;
		if (DmSpawnService.BestSpawnIndex(points, enemies) != 1) pickOk = 0;
		if (DmSpawnService.BestSpawnIndex(null, enemies) != -1) pickOk = 0;
		int emptyPick = DmSpawnService.BestSpawnIndex(points, new array<vector>);
		if (emptyPick < 0 || emptyPick > 1) pickOk = 0;
		Print("[DM] fixture DmSpawnService.BestSpawnIndex: expected=1 got=" + pickOk.ToString() + " " + DmFixture.Verdict(pickOk == 1));

		// Death avoidance: dying at the far point must exclude it even though
		// the enemy stands on the near point (avoidance beats enemy distance);
		// an avoid radius covering every point falls back to the full set.
		int avoidOk = 1;
		if (DmSpawnService.BestSpawnIndexAvoiding(points, enemies, Vector(500, 0, 0), 75) != 0) avoidOk = 0;
		if (DmSpawnService.BestSpawnIndexAvoiding(points, enemies, Vector(250, 0, 0), 10000) != 1) avoidOk = 0;
		if (DmSpawnService.BestSpawnIndexAvoiding(points, enemies, Vector(0, 0, 0), 0) != 1) avoidOk = 0;
		Print("[DM] fixture DmSpawnService avoid-death: expected=1 got=" + avoidOk.ToString() + " " + DmFixture.Verdict(avoidOk == 1));

		// Death memory: one-shot consume, freshness-gated.
		DmSpawnService memProbe = new DmSpawnService();
		int memOk = 1;
		memProbe.NoteDeathById("p1", Vector(100, 0, 200), 1000.0);
		vector recalled = memProbe.ConsumeDeathPosById("p1", 1010.0);
		if (recalled[0] != 100 || recalled[2] != 200) memOk = 0;
		vector second = memProbe.ConsumeDeathPosById("p1", 1011.0);
		if (second[0] != 0 || second[2] != 0) memOk = 0;
		memProbe.NoteDeathById("p2", Vector(50, 0, 50), 1000.0);
		vector stale = memProbe.ConsumeDeathPosById("p2", 1000.0 + DEATH_MEMORY_SECONDS + 1.0);
		if (stale[0] != 0 || stale[2] != 0) memOk = 0;
		if (memProbe.ConsumeDeathPosById("never-died", 1000.0) != Vector(0, 0, 0)) memOk = 0;
		Print("[DM] fixture DmSpawnService death memory: expected=1 got=" + memOk.ToString() + " " + DmFixture.Verdict(memOk == 1));

		// Protection window: open for exactly its configured span, per player;
		// a zero-second window never opens.
		DmSpawnService protProbe = new DmSpawnService();
		int protOk = 1;
		protProbe.StartProtectionById("p1", 100.0, 3);
		if (!protProbe.IsProtectedById("p1", 100.0)) protOk = 0;
		if (!protProbe.IsProtectedById("p1", 102.9)) protOk = 0;
		if (protProbe.IsProtectedById("p1", 103.0)) protOk = 0;
		if (protProbe.IsProtectedById("p2", 101.0)) protOk = 0;
		protProbe.StartProtectionById("p3", 100.0, 0);
		if (protProbe.IsProtectedById("p3", 100.0)) protOk = 0;
		Print("[DM] fixture DmSpawnService protection window: expected=1 got=" + protOk.ToString() + " " + DmFixture.Verdict(protOk == 1));
	}
}
