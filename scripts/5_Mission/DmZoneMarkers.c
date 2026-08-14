// In-world zone edge markers: red smoke pillars along the boundary arc
// nearest the player, client-side only, driven entirely by the synced zone
// data. The full ring would be 30+ live emitters, so only a sliding window
// of slots around the player's bearing is lit at once; pillar positions snap
// to a fixed arc grid so they never slide as the player strafes.
class DmZoneMarkers
{
	private static ref DmZoneMarkers s_Instance;

	// Band around the boundary (either side) in which markers render.
	static const float SHOW_RANGE_M = 60.0;
	// Arc distance between pillars.
	static const float SPACING_M = 30.0;
	// Pillars kept lit each side of the player's own arc slot.
	static const int WINDOW_SLOTS = 3;

	private ref map<int, Particle> m_Pillars = new map<int, Particle>;

	static DmZoneMarkers GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new DmZoneMarkers();
		}
		return s_Instance;
	}

	// Total slots on the ring for this zone geometry. Pure; fixtures.
	static int TotalSlots(float radius, float spacingMeters)
	{
		if (radius <= 0 || spacingMeters <= 0) return 0;
		int slots = Math.Round(Math.PI2 * radius / spacingMeters);
		if (slots < 1) slots = 1;
		return slots;
	}

	// The arc slot the player's bearing from center snaps to. Pure; fixtures.
	static int ArcSlot(float px, float pz, float cx, float cz, float radius, float spacingMeters)
	{
		int total = TotalSlots(radius, spacingMeters);
		if (total == 0) return 0;
		float theta = Math.Atan2(pz - cz, px - cx);
		int slot = Math.Round(theta / (Math.PI2 / total));
		return WrapSlot(slot, total);
	}

	// Modulo that lands in [0, total). Pure; fixtures.
	static int WrapSlot(int slot, int total)
	{
		if (total <= 0) return 0;
		int wrapped = slot % total;
		if (wrapped < 0) wrapped = wrapped + total;
		return wrapped;
	}

	// Called from the HUD controller's 500 ms tick (client only).
	void Update(DmClientState state)
	{
		bool active = false;
		int centerSlot = 0;
		int total = 0;

		if (state.m_ZoneRadius > 0 && (state.m_Phase == DmPhase.LIVE || state.m_Phase == DmPhase.COUNTDOWN))
		{
			Man ownPlayer = GetGame().GetPlayer();
			if (ownPlayer)
			{
				vector ownPos = ownPlayer.GetPosition();
				float overshoot = DmZoneService.OutDistance2D(ownPos[0], ownPos[2], state.m_ZoneCX, state.m_ZoneCZ, state.m_ZoneRadius);
				if (Math.AbsFloat(overshoot) <= SHOW_RANGE_M)
				{
					active = true;
					total = TotalSlots(state.m_ZoneRadius, SPACING_M);
					centerSlot = ArcSlot(ownPos[0], ownPos[2], state.m_ZoneCX, state.m_ZoneCZ, state.m_ZoneRadius, SPACING_M);
				}
			}
		}

		if (!active)
		{
			ClearAll();
			return;
		}

		// Which slots should be lit this tick.
		map<int, bool> wanted = new map<int, bool>;
		for (int offset = -WINDOW_SLOTS; offset <= WINDOW_SLOTS; offset++)
		{
			wanted.Set(WrapSlot(centerSlot + offset, total), true);
		}

		// Stop pillars that left the window.
		array<int> stale = new array<int>;
		for (int litIdx = 0; litIdx < m_Pillars.Count(); litIdx++)
		{
			int litSlot = m_Pillars.GetKey(litIdx);
			bool keep;
			if (!wanted.Find(litSlot, keep)) stale.Insert(litSlot);
		}
		for (int staleIdx = 0; staleIdx < stale.Count(); staleIdx++)
		{
			StopPillar(stale[staleIdx]);
		}

		// Light missing slots - at most ONE spawn per tick. Spawning the whole
		// window in a single frame hitched clients hard enough to read as a
		// freeze (live playtest 08-14); staggered, the window fills over ~3 s
		// as you approach and nobody notices the cost.
		for (int wantIdx = 0; wantIdx < wanted.Count(); wantIdx++)
		{
			int slot = wanted.GetKey(wantIdx);
			Particle existing;
			if (m_Pillars.Find(slot, existing)) continue;

			float theta = slot * (Math.PI2 / total);
			float markX = state.m_ZoneCX + state.m_ZoneRadius * Math.Cos(theta);
			float markZ = state.m_ZoneCZ + state.m_ZoneRadius * Math.Sin(theta);
			vector markPos = Vector(markX, GetGame().SurfaceY(markX, markZ), markZ);
			Particle pillar = ParticleManager.GetInstance().PlayInWorld(ParticleList.GRENADE_M18_RED_LOOP, markPos);
			if (pillar) m_Pillars.Set(slot, pillar);
			break;
		}
	}

	// First-ever particle spawn also pays the asset load (textures, effect
	// data) - milliseconds of stall that must not land mid-firefight. Burn
	// it at mission start instead: one emitter far underground, stopped two
	// seconds later, before the player is anywhere near combat.
	private Particle m_PrewarmParticle;

	void Prewarm()
	{
		if (m_PrewarmParticle) return;
		m_PrewarmParticle = ParticleManager.GetInstance().PlayInWorld(ParticleList.GRENADE_M18_RED_LOOP, Vector(0, -200, 0));
		GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(StopPrewarm, 2000, false);
	}

	void StopPrewarm()
	{
		if (m_PrewarmParticle)
		{
			m_PrewarmParticle.Stop();
			m_PrewarmParticle = null;
		}
	}

	private void StopPillar(int slot)
	{
		Particle pillar;
		if (m_Pillars.Find(slot, pillar) && pillar)
		{
			pillar.Stop();
		}
		m_Pillars.Remove(slot);
	}

	void ClearAll()
	{
		for (int pillarIdx = 0; pillarIdx < m_Pillars.Count(); pillarIdx++)
		{
			Particle pillar = m_Pillars.GetElement(pillarIdx);
			if (pillar) pillar.Stop();
		}
		m_Pillars.Clear();
	}

	static void SelfTest()
	{
		int slotOk = 1;
		// 250 m radius / 30 m spacing: 52 pillars ring the arena.
		if (TotalSlots(250.0, 30.0) != 52) slotOk = 0;
		// Due east of center = angle 0 = slot 0; due north lands a quarter in.
		if (ArcSlot(4850.0, 10000.0, 4600.0, 10000.0, 250.0, 30.0) != 0) slotOk = 0;
		if (ArcSlot(4600.0, 10250.0, 4600.0, 10000.0, 250.0, 30.0) != 13) slotOk = 0;
		// Wrap keeps slots in [0, total) from either direction.
		if (WrapSlot(-1, 52) != 51) slotOk = 0;
		if (WrapSlot(53, 52) != 1) slotOk = 0;
		if (WrapSlot(0, 0) != 0) slotOk = 0;
		// Degenerate geometry never divides by zero.
		if (TotalSlots(0, 30.0) != 0) slotOk = 0;
		Print("[DM] fixture DmZoneMarkers arc slots: expected=1 got=" + slotOk.ToString() + " " + DmFixture.Verdict(slotOk == 1));
	}
}
