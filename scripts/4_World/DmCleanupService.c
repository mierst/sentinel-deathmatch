// Corpse cleanup. CE's own cleanup is suppressed near players
// (CleanupAvoidance), which in a dense arena means bodies outlive their
// lifetime - so the mod is the real cleanup and CE is only the backstop.
//
// Deletions are spread across ticks (MaxDeletesPerTick) because object
// deletion replicates through the engine's frame-batched network queues; a
// team wipe swept in one frame causes visible pop-in.
class DmCorpseRecord
{
	EntityAI Body;
	float Deadline;

	void DmCorpseRecord(EntityAI body, float deadline)
	{
		Body = body;
		Deadline = deadline;
	}
}

class DmCleanupService
{
	private static ref DmCleanupService s_Instance;

	private ref array<ref DmCorpseRecord> m_Corpses = new array<ref DmCorpseRecord>;
	private ref Timer m_SweepTimer;
	private bool m_Started = false;

	static DmCleanupService GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new DmCleanupService();
		}
		return s_Instance;
	}

	void Start()
	{
		if (m_Started) return;
		m_Started = true;
		m_SweepTimer = new Timer(CALL_CATEGORY_SYSTEM);
		m_SweepTimer.Run(1.0, this, "OnSweep", null, true);
	}

	void RegisterCorpse(EntityAI body)
	{
		if (!body) return;
		float deadline = GetGame().GetTickTime() + DmConfig.GetInstance().GetCorpseLifetimeSeconds();
		m_Corpses.Insert(new DmCorpseRecord(body, deadline));
	}

	// Round-start sweep: loose items on the ground inside the chosen zone
	// (last round's dropped guns, mags, bandages) are queued for immediate
	// deletion through the same rate-limited pipe as corpses. Skips anything
	// held by a player, anything inside a container hierarchy, and the arena
	// system's own placed objects.
	void SweepGroundItems(DmZoneData zone)
	{
		if (!zone) return;

		vector sweepCenter = Vector(zone.CenterX, GetGame().SurfaceY(zone.CenterX, zone.CenterZ), zone.CenterZ);
		array<Object> nearby = new array<Object>;
		array<CargoBase> proxyCargos = new array<CargoBase>;
		GetGame().GetObjectsAtPosition(sweepCenter, zone.Radius + zone.WarnMargin, nearby, proxyCargos);

		int swept = 0;
		float nowSeconds = GetGame().GetTickTime();
		for (int objIdx = 0; objIdx < nearby.Count(); objIdx++)
		{
			ItemBase looseItem = ItemBase.Cast(nearby[objIdx]);
			if (!looseItem) continue;
			if (looseItem.GetHierarchyRootPlayer()) continue; // in someone's hands/inventory
			if (looseItem.GetHierarchyParent()) continue;     // inside a container
			if (DmArenaService.GetInstance().IsArenaObject(looseItem)) continue;
			// Deadline of now = deleted by the very next sweep ticks.
			m_Corpses.Insert(new DmCorpseRecord(looseItem, nowSeconds));
			swept = swept + 1;
		}
		if (swept > 0)
		{
			Print("[DM] cleanup: " + swept.ToString() + " loose ground items queued for round-start sweep");
		}
	}

	int GetPendingCount() { return m_Corpses.Count(); }

	void OnSweep()
	{
		if (m_Corpses.Count() == 0) return;

		float nowSeconds = GetGame().GetTickTime();
		int deletesLeft = DmConfig.GetInstance().GetMaxDeletesPerTick();

		// Walk backwards so removal doesn't shift unvisited entries.
		for (int recIdx = m_Corpses.Count() - 1; recIdx >= 0; recIdx--)
		{
			if (deletesLeft <= 0) break;

			DmCorpseRecord rec = m_Corpses[recIdx];
			if (!rec.Body)
			{
				// Already gone (CE backstop or restart) - drop the record.
				m_Corpses.Remove(recIdx);
				continue;
			}
			if (nowSeconds >= rec.Deadline)
			{
				GetGame().ObjectDelete(rec.Body);
				m_Corpses.Remove(recIdx);
				deletesLeft = deletesLeft - 1;
			}
		}
	}

	// ROUNDEND: pull every remaining deadline forward so the scoreboard
	// window clears the field, still rate-limited by the sweep.
	void ExpireAll()
	{
		float nowSeconds = GetGame().GetTickTime();
		for (int recIdx = 0; recIdx < m_Corpses.Count(); recIdx++)
		{
			m_Corpses[recIdx].Deadline = nowSeconds;
		}
	}

	static void SelfTest()
	{
		DmCleanupService probe = new DmCleanupService();
		int emptyOk = 1;
		if (probe.GetPendingCount() != 0) emptyOk = 0;
		probe.RegisterCorpse(null);
		if (probe.GetPendingCount() != 0) emptyOk = 0;
		Print("[DM] fixture DmCleanupService null-safe: expected=1 got=" + emptyOk.ToString() + " " + DmFixture.Verdict(emptyOk == 1));
	}
}
