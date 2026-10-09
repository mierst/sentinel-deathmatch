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
	bool Gun;
	bool LooseOnly;

	void DmCorpseRecord(EntityAI body, float deadline, bool gun = false, bool looseOnly = false)
	{
		Body = body;
		Deadline = deadline;
		Gun = gun;
		LooseOnly = looseOnly;
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

	void RegisterCorpse(EntityAI body, Weapon_Base droppedGun = null)
	{
		if (!body) return;
		float deathTime = GetGame().GetTickTime();
		float deadline = deathTime + DmConfig.GetInstance().GetCorpseLifetimeSeconds();
		m_Corpses.Insert(new DmCorpseRecord(body, deadline));
		RegisterDeathGun(droppedGun, deathTime);
		array<EntityAI> inventory = new array<EntityAI>;
		body.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inventory);
		for (int gunIdx = 0; gunIdx < inventory.Count(); gunIdx++)
		{
			Weapon_Base gun = Weapon_Base.Cast(inventory[gunIdx]);
			if (!gun) continue;
			if (gun != droppedGun) RegisterDeathGun(gun, deathTime);
			// Corpse deletion must not impose a second, shorter gun lifetime.
			body.ServerDropEntity(gun);
		}
	}

	private void RegisterDeathGun(Weapon_Base gun, float deathTime)
	{
		if (!gun) return;
		string mode = DmConfig.GetInstance().GetGunCleanupMode();
		if (mode == "server") return;
		m_Corpses.Insert(new DmCorpseRecord(gun, GunDeadline(mode, deathTime), true));
	}

	// A failed inventory move must not cause corpse deletion to take its guns
	// with it. Retry on the existing sweep instead of forcing a destructive move.
	private bool ReleaseCorpseGuns(EntityAI body)
	{
		array<EntityAI> remaining = new array<EntityAI>;
		body.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, remaining);
		bool released = true;
		for (int releaseIdx = 0; releaseIdx < remaining.Count(); releaseIdx++)
		{
			Weapon_Base remainingGun = Weapon_Base.Cast(remaining[releaseIdx]);
			if (!remainingGun) continue;
			body.ServerDropEntity(remainingGun);
			if (remainingGun.GetHierarchyRootPlayer() == body) released = false;
		}
		return released;
	}

	private bool ContainsGun(EntityAI container)
	{
		array<EntityAI> contents = new array<EntityAI>;
		container.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, contents);
		for (int contentIdx = 0; contentIdx < contents.Count(); contentIdx++)
		{
			if (Weapon_Base.Cast(contents[contentIdx])) return true;
		}
		return false;
	}

	// Countdown clears non-gun ground items. Round end clears ground guns
	// only in round_end mode. Both use the existing rate-limited deletion pipe.
	void SweepGroundItems(DmZoneData zone, bool roundEnd = false)
	{
		if (!zone) return;
		string gunMode = DmConfig.GetInstance().GetGunCleanupMode();
		if (roundEnd && !ShouldSweepGun(gunMode, true)) return;

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
			bool isGun = Weapon_Base.Cast(looseItem) != null;
			if (isGun && !ShouldSweepGun(gunMode, roundEnd)) continue;
			if (roundEnd && !isGun) continue;
			// Deadline of now = deleted by the very next sweep ticks.
			m_Corpses.Insert(new DmCorpseRecord(looseItem, nowSeconds, isGun, true));
			swept = swept + 1;
		}
		if (swept > 0)
		{
			Print("[DM] cleanup: " + swept.ToString() + " loose ground items queued for sweep");
		}
	}

	int GetPendingCount() { return m_Corpses.Count(); }

	static bool ShouldSweepGun(string mode, bool roundEnd)
	{
		return mode == "round_end" && roundEnd;
	}

	static float GunDeadline(string mode, float deathTime)
	{
		if (mode == "player_death") return deathTime + 10.0;
		return -1.0; // Round-end records wait for ExpireAll; server mode is unqueued.
	}

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
			// Ownership can change after queueing. Cancel a looted gun's record
			// even before its deadline so a later drop does not revive that timer.
			if (rec.Gun)
			{
				PlayerBase owner = PlayerBase.Cast(rec.Body.GetHierarchyRootPlayer());
				if ((owner && owner.IsAlive()) || (!owner && rec.Body.GetHierarchyParent()))
				{
					m_Corpses.Remove(recIdx);
					continue;
				}
			}
			if (rec.Deadline >= 0 && nowSeconds >= rec.Deadline)
			{
				if (rec.LooseOnly && rec.Body.GetHierarchyParent())
				{
					m_Corpses.Remove(recIdx);
					continue;
				}
				// A ground backpack/case must not bypass the gun policy by
				// taking its contents with it during the non-gun countdown sweep.
				if (rec.LooseOnly && !rec.Gun && ContainsGun(rec.Body))
				{
					m_Corpses.Remove(recIdx);
					continue;
				}
				if (PlayerBase.Cast(rec.Body) && !ReleaseCorpseGuns(rec.Body)) continue;
				GetGame().ObjectDelete(rec.Body);
				m_Corpses.Remove(recIdx);
				deletesLeft = deletesLeft - 1;
			}
		}
	}

	// Round end expires corpses and round_end guns. Death-mode guns keep
	// their original ten-second deadline even when a round finishes early.
	void ExpireAll()
	{
		float nowSeconds = GetGame().GetTickTime();
		string gunMode = DmConfig.GetInstance().GetGunCleanupMode();
		for (int recIdx = 0; recIdx < m_Corpses.Count(); recIdx++)
		{
			if (m_Corpses[recIdx].Gun && gunMode != "round_end") continue;
			m_Corpses[recIdx].Deadline = nowSeconds;
		}
	}

	static void SelfTest()
	{
		int gunPolicyOk = 1;
		if (ShouldSweepGun("server", false)) gunPolicyOk = 0;
		if (ShouldSweepGun("server", true)) gunPolicyOk = 0;
		if (ShouldSweepGun("round_end", false)) gunPolicyOk = 0;
		if (!ShouldSweepGun("round_end", true)) gunPolicyOk = 0;
		if (ShouldSweepGun("player_death", false)) gunPolicyOk = 0;
		if (ShouldSweepGun("player_death", true)) gunPolicyOk = 0;
		Print("[DM] fixture DmCleanupService gun sweep policy: expected=1 got=" + gunPolicyOk.ToString() + " " + DmFixture.Verdict(gunPolicyOk == 1));
		int gunDeadlineOk = 1;
		if (GunDeadline("player_death", 100.0) != 110.0) gunDeadlineOk = 0;
		if (GunDeadline("server", 100.0) != -1.0) gunDeadlineOk = 0;
		if (GunDeadline("round_end", 100.0) != -1.0) gunDeadlineOk = 0;
		Print("[DM] fixture DmCleanupService gun death deadline: expected=1 got=" + gunDeadlineOk.ToString() + " " + DmFixture.Verdict(gunDeadlineOk == 1));
		DmCleanupService probe = new DmCleanupService();
		int emptyOk = 1;
		if (probe.GetPendingCount() != 0) emptyOk = 0;
		probe.RegisterCorpse(null);
		if (probe.GetPendingCount() != 0) emptyOk = 0;
		Print("[DM] fixture DmCleanupService null-safe: expected=1 got=" + emptyOk.ToString() + " " + DmFixture.Verdict(emptyOk == 1));
	}
}
