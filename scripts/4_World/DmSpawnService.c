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
		if (!points || points.Count() == 0) return -1;
		if (!enemyPositions || enemyPositions.Count() == 0)
		{
			return Math.RandomInt(0, points.Count());
		}

		int bestIdx = 0;
		float bestScore = -1;
		for (int pointIdx = 0; pointIdx < points.Count(); pointIdx++)
		{
			DmSpawnPointData sp = points[pointIdx];
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
				bestIdx = pointIdx;
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

	// Choose a spawn position in the active zone, away from living enemies.
	vector PickSpawnPosition(out float outYaw)
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

		int chosenIdx = BestSpawnIndex(zone.SpawnPoints, enemyPositions);
		if (chosenIdx < 0) chosenIdx = 0;
		DmSpawnPointData chosen = zone.SpawnPoints[chosenIdx];
		outYaw = chosen.Yaw;
		return ResolveSpawnPos(chosen);
	}

	// EEKilled entry: schedule the server-driven respawn.
	void ScheduleRespawn(PlayerIdentity identity)
	{
		if (!identity) return;
		int delayMs = DmConfig.GetInstance().GetRespawnDelaySeconds() * 1000;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DoRespawn, delayMs, false, identity);
	}

	void DoRespawn(PlayerIdentity identity)
	{
		if (!identity) return;

		float spawnYaw;
		vector spawnPos = PickSpawnPosition(spawnYaw);

		// 4_World cannot reference MissionServer (5_Mission type), so this
		// uses the same 3_Game primitives MissionServer.CreateCharacter
		// wraps: CreatePlayer + SelectPlayer.
		Entity playerEnt = GetGame().CreatePlayer(identity, GetGame().CreateRandomPlayer(), spawnPos, 0, "NONE");
		PlayerBase fresh = PlayerBase.Cast(playerEnt);
		if (!fresh)
		{
			Print("[DM] WARNING: respawn CreatePlayer failed for " + identity.GetName());
			return;
		}
		GetGame().SelectPlayer(identity, fresh);
		fresh.SetOrientation(Vector(spawnYaw, 0, 0));

		ApplyProtection(fresh);
		DmLoadoutFactory.GetInstance().Apply(fresh, DmVoteService.GetInstance().GetActivePresetIndex());
	}

	void ApplyProtection(PlayerBase pb)
	{
		int protectSeconds = DmConfig.GetInstance().GetSpawnProtectSeconds();
		if (protectSeconds <= 0) return;

		PlayerIdentity ident = pb.GetIdentity();
		if (!ident) return;

		pb.SetAllowDamage(false);
		m_ProtectUntil.Set(ident.GetPlainId(), GetGame().GetTickTime() + protectSeconds);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(EndProtection, protectSeconds * 1000, false, pb);
	}

	void EndProtection(PlayerBase pb)
	{
		if (!pb) return;
		pb.SetAllowDamage(true);
		PlayerIdentity ident = pb.GetIdentity();
		if (ident)
		{
			m_ProtectUntil.Remove(ident.GetPlainId());
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
	}
}
