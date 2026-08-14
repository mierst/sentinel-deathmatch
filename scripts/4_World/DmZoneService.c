// Active-zone tracking and boundary enforcement.
//
// No trigger entities: membership is a 2D distance check per player on the
// round engine's 500 ms tick. The math is a pure static so fixtures cover it.
class DmZoneService
{
	private static ref DmZoneService s_Instance;

	private DmZoneData m_ActiveZone;

	// "countdown" enforcement state: when each player was first seen outside,
	// and which deaths this service caused (consumed by the round engine so
	// the killfeed can say "left the zone" instead of "died").
	private ref map<string, float> m_OutsideSince = new map<string, float>;
	private ref map<string, bool> m_PendingZoneKills = new map<string, bool>;

	static DmZoneService GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new DmZoneService();
		}
		return s_Instance;
	}

	void SetActiveZone(DmZoneData zone)
	{
		m_ActiveZone = zone;
		m_OutsideSince.Clear();
		m_PendingZoneKills.Clear();
	}

	// True exactly once per zone-enforcement death of this player.
	bool ConsumeZoneKill(string playerId)
	{
		bool pending;
		if (!m_PendingZoneKills.Find(playerId, pending)) return false;
		m_PendingZoneKills.Remove(playerId);
		return pending;
	}
	DmZoneData GetActiveZone() { return m_ActiveZone; }

	string GetActiveZoneName()
	{
		if (!m_ActiveZone) return "";
		return m_ActiveZone.Name;
	}

	// Meters OUTSIDE the boundary (2D). Negative while inside. Pure; fixtures.
	static float OutDistance2D(float px, float pz, float cx, float cz, float radius)
	{
		float dx = px - cx;
		float dz = pz - cz;
		return Math.Sqrt(dx * dx + dz * dz) - radius;
	}

	// Called from the round engine tick during LIVE with the shared player
	// scratch list. dtSeconds is the tick interval.
	void Enforce(array<Man> players, float dtSeconds)
	{
		if (!m_ActiveZone) return;

		for (int playerIdx = 0; playerIdx < players.Count(); playerIdx++)
		{
			PlayerBase pb = PlayerBase.Cast(players[playerIdx]);
			if (!pb || !pb.IsAlive()) continue;

			vector pos = pb.GetPosition();
			float overshoot = OutDistance2D(pos[0], pos[2], m_ActiveZone.CenterX, m_ActiveZone.CenterZ, m_ActiveZone.Radius);

			string playerId = "";
			PlayerIdentity ident = pb.GetIdentity();
			if (ident) playerId = ident.GetPlainId();

			if (overshoot <= 0)
			{
				// Back inside: forgive any running grace timer.
				if (playerId != "") m_OutsideSince.Remove(playerId);
				continue;
			}

			if (m_ActiveZone.Enforcement == "teleport")
			{
				TeleportToNearestSpawn(pb);
			}
			else if (m_ActiveZone.Enforcement == "countdown")
			{
				if (playerId == "") continue;
				float nowSeconds = GetGame().GetTickTime();
				float sinceSeconds;
				if (!m_OutsideSince.Find(playerId, sinceSeconds))
				{
					m_OutsideSince.Set(playerId, nowSeconds);
					continue;
				}
				if (nowSeconds - sinceSeconds >= m_ActiveZone.OutOfZoneKillSeconds)
				{
					m_OutsideSince.Remove(playerId);
					m_PendingZoneKills.Set(playerId, true);
					pb.SetHealth("GlobalHealth", "Health", 0);
				}
			}
			else
			{
				// Soft wall: damage scales with how far out the player is,
				// so grazing the edge stings and running costs a life.
				float scale = 1.0 + (overshoot / 25.0);
				pb.DecreaseHealth("GlobalHealth", "Health", m_ActiveZone.OutOfZoneDmgPerSec * dtSeconds * scale);
			}
		}
	}

	void TeleportToNearestSpawn(PlayerBase pb)
	{
		if (!m_ActiveZone || m_ActiveZone.SpawnPoints.Count() == 0) return;

		vector pos = pb.GetPosition();
		int bestIdx = 0;
		float bestDistSq = 9999999999.0;
		for (int spIdx = 0; spIdx < m_ActiveZone.SpawnPoints.Count(); spIdx++)
		{
			DmSpawnPointData sp = m_ActiveZone.SpawnPoints[spIdx];
			float ddx = pos[0] - sp.X;
			float ddz = pos[2] - sp.Z;
			float distSq = ddx * ddx + ddz * ddz;
			if (distSq < bestDistSq)
			{
				bestDistSq = distSq;
				bestIdx = spIdx;
			}
		}
		pb.SetPosition(DmSpawnService.ResolveSpawnPos(m_ActiveZone.SpawnPoints[bestIdx]));
	}

	static void SelfTest()
	{
		int mathOk = 1;
		// 300 m from center, radius 250 -> 50 m outside.
		float outVal = DmZoneService.OutDistance2D(300, 0, 0, 0, 250);
		if (outVal < 49.9 || outVal > 50.1) mathOk = 0;
		// Inside: negative.
		float inVal = DmZoneService.OutDistance2D(10, 10, 0, 0, 250);
		if (inVal >= 0) mathOk = 0;
		// Elevation must not matter (2D check): args carry no Y at all.
		Print("[DM] fixture DmZoneService.OutDistance2D: expected=1 got=" + mathOk.ToString() + " " + DmFixture.Verdict(mathOk == 1));
	}
}
