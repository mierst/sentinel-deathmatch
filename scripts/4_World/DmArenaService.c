// Operator-authored arenas: DayZ Editor .dze object sets per zone.
//
// A zone's DzeFile references a JSON .dze in $profile:SentinelDeathmatch\arenas\.
// The schema classes below are an INDEPENDENT implementation from format
// knowledge only - no code from DayZ Editor or any loader mod (see the
// project license notes). Spawn mechanics mirror vanilla's own
// ObjectSpawnerHandler, minus the pathgraph work: arenas host zero AI.
//
// Lifecycle is bubble-aware: the winning arena's objects spawn when the vote
// resolves - before anyone is teleported there, so creation replicates into
// nobody's network bubble - and a retired arena's objects delete during the
// NEXT cycle's vote window, by which point everyone has been away from that
// bubble for a full round. Same-arena repeat votes touch nothing, and a
// retired-but-undeleted arena that wins again is resurrected, not respawned.
class DmDzeObject
{
	string Type = "";
	string DisplayName = "";
	ref array<float> Position = new array<float>;
	ref array<float> Orientation = new array<float>;
	float Scale = 1.0;
	bool EditorOnly = false;
	bool Simulate = false;
}

class DmDzeSave
{
	string MapName = "";
	ref array<ref DmDzeObject> EditorObjects = new array<ref DmDzeObject>;
}

class DmArenaService
{
	static string ARENAS_DIR = "$profile:SentinelDeathmatch\\arenas";
	// Soft budget: warn above this, still spawn.
	static const int WARN_OBJECTS = 300;

	private static ref DmArenaService s_Instance;

	// zone name -> validated spawnable object set.
	private ref map<string, ref array<ref DmDzeObject>> m_SetsByZone = new map<string, ref array<ref DmDzeObject>>;

	// Materialized world objects.
	private string m_ActiveZoneName = "";
	private ref array<Object> m_ActiveObjects = new array<Object>;
	// zone name -> its still-standing objects awaiting deletion.
	private ref map<string, ref array<Object>> m_RetiredByZone = new map<string, ref array<Object>>;
	// Spawns pending for the active zone, drained rate-limited per tick.
	private ref array<ref DmDzeObject> m_SpawnQueue = new array<ref DmDzeObject>;

	static DmArenaService GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new DmArenaService();
		}
		return s_Instance;
	}

	// ---- boot ----

	// Called from the round engine's Start after zones.json validated. An
	// arena problem disables its ZONE (loud, like a bad preset) so a vote can
	// never land on an arena that cannot materialize.
	void LoadAll()
	{
		m_SetsByZone.Clear();
		DmZonesConfig zones = DmZonesConfig.GetInstance();

		// Snapshot first: disabling a zone mutates the enabled list.
		array<string> zoneNames = new array<string>;
		array<string> dzeFiles = new array<string>;
		for (int zoneIdx = 0; zoneIdx < zones.GetEnabledCount(); zoneIdx++)
		{
			DmZoneData zone = zones.GetEnabledZone(zoneIdx);
			if (!zone || zone.DzeFile == "") continue;
			zoneNames.Insert(zone.Name);
			dzeFiles.Insert(zone.DzeFile);
		}

		int totalObjects = 0;
		for (int loadIdx = 0; loadIdx < zoneNames.Count(); loadIdx++)
		{
			string zoneName = zoneNames[loadIdx];
			string dzeFile = dzeFiles[loadIdx];

			if (!ValidArenaFilename(dzeFile))
			{
				Print("[DM] arenas: zone '" + zoneName + "' DzeFile '" + dzeFile + "' is not a plain filename - zone disabled");
				zones.DisableZoneByName(zoneName);
				continue;
			}
			string dzePath = ARENAS_DIR + "\\" + dzeFile;
			if (!FileExist(dzePath))
			{
				Print("[DM] arenas: zone '" + zoneName + "' file not found: " + dzePath + " - zone disabled");
				zones.DisableZoneByName(zoneName);
				continue;
			}

			DmDzeSave save = new DmDzeSave();
			JsonFileLoader<DmDzeSave>.JsonLoadFile(dzePath, save);
			if (save.EditorObjects.Count() == 0)
			{
				Print("[DM] arenas: zone '" + zoneName + "' " + dzeFile + " has no objects (binary .dze? re-save as JSON in the Editor) - zone disabled");
				zones.DisableZoneByName(zoneName);
				continue;
			}

			array<ref DmDzeObject> spawnable = new array<ref DmDzeObject>;
			string badClass = CollectSpawnable(save.EditorObjects, spawnable);
			if (badClass != "")
			{
				Print("[DM] arenas: zone '" + zoneName + "' references unknown classname '" + badClass + "' (mod not loaded?) - zone disabled");
				zones.DisableZoneByName(zoneName);
				continue;
			}
			if (spawnable.Count() > DmConfig.GetInstance().GetMaxArenaObjects())
			{
				Print("[DM] arenas: zone '" + zoneName + "' has " + spawnable.Count().ToString() + " objects (MaxArenaObjects " + DmConfig.GetInstance().GetMaxArenaObjects().ToString() + ") - zone disabled");
				zones.DisableZoneByName(zoneName);
				continue;
			}
			if (spawnable.Count() > WARN_OBJECTS)
			{
				Print("[DM] arenas: zone '" + zoneName + "' has " + spawnable.Count().ToString() + " objects - heavy; consider trimming");
			}

			m_SetsByZone.Set(zoneName, spawnable);
			totalObjects = totalObjects + spawnable.Count();
			Print("[DM] arenas: zone '" + zoneName + "' <- " + dzeFile + " (" + spawnable.Count().ToString() + " objects)");
		}
		Print("[DM] arenas: " + m_SetsByZone.Count().ToString() + " arena object sets loaded, " + totalObjects.ToString() + " objects total");
	}

	// Plain filename only: the profile arenas dir is the entire namespace.
	static bool ValidArenaFilename(string fileName)
	{
		if (fileName == "") return false;
		if (fileName.Contains("..")) return false;
		if (fileName.Contains("/")) return false;
		if (fileName.Contains("\\")) return false;
		return true;
	}

	// Filter to what v1 spawns; returns the first unresolvable classname, or
	// "" when clean. EditorOnly markers and empty types are skipped; .p3d
	// path placements are skipped with a log (classname objects first).
	private string CollectSpawnable(array<ref DmDzeObject> rawObjects, array<ref DmDzeObject> outSpawnable)
	{
		for (int objIdx = 0; objIdx < rawObjects.Count(); objIdx++)
		{
			DmDzeObject dzeObj = rawObjects[objIdx];
			if (!dzeObj || dzeObj.EditorOnly || dzeObj.Type == "") continue;
			if (dzeObj.Type.Contains("/") || dzeObj.Type.Contains("\\"))
			{
				Print("[DM] arenas: .p3d placement '" + dzeObj.Type + "' skipped (classname objects only for now)");
				continue;
			}
			if (dzeObj.Position.Count() < 3) continue;
			if (!GetGame().ConfigIsExisting("CfgVehicles " + dzeObj.Type))
			{
				return dzeObj.Type;
			}
			outSpawnable.Insert(dzeObj);
		}
		return "";
	}

	// ---- round lifecycle (engine-driven) ----

	// Right after the vote resolves, before anyone teleports. Spawns queue
	// up and drain over the countdown - into an empty network bubble.
	void OnArenaChosen(string zoneName)
	{
		if (zoneName == m_ActiveZoneName) return;

		// Retire the current arena's objects; they fall a full cycle later.
		if (m_ActiveZoneName != "" && m_ActiveObjects.Count() > 0)
		{
			m_RetiredByZone.Set(m_ActiveZoneName, m_ActiveObjects);
			m_ActiveObjects = new array<Object>;
		}
		m_ActiveZoneName = zoneName;
		m_SpawnQueue.Clear();

		// Re-elected before its objects fell: resurrect, don't respawn.
		array<Object> retiredSet;
		if (m_RetiredByZone.Find(zoneName, retiredSet))
		{
			m_ActiveObjects = retiredSet;
			m_RetiredByZone.Remove(zoneName);
			Print("[DM] arenas: '" + zoneName + "' re-elected - " + m_ActiveObjects.Count().ToString() + " objects resurrected");
			return;
		}

		array<ref DmDzeObject> objectSet;
		if (!m_SetsByZone.Find(zoneName, objectSet)) return;
		for (int queueIdx = 0; queueIdx < objectSet.Count(); queueIdx++)
		{
			m_SpawnQueue.Insert(objectSet[queueIdx]);
		}
		Print("[DM] arenas: '" + zoneName + "' chosen - " + m_SpawnQueue.Count().ToString() + " objects queued");
	}

	// Engine tick hook. Spawns drain in every phase (countdown mostly);
	// retired arenas fall only during VOTING/IDLE, when their bubbles have
	// been empty for a full round.
	void Tick(int phase)
	{
		DrainSpawns(DmConfig.GetInstance().GetMaxArenaSpawnsPerTick());
		if (phase == DmPhase.VOTING || phase == DmPhase.IDLE)
		{
			DrainDeletes(DmConfig.GetInstance().GetMaxArenaDeletesPerTick());
		}
	}

	private void DrainSpawns(int maxThisTick)
	{
		int spawned = 0;
		while (m_SpawnQueue.Count() > 0 && spawned < maxThisTick)
		{
			DmDzeObject dzeObj = m_SpawnQueue[0];
			m_SpawnQueue.RemoveOrdered(0);
			SpawnOne(dzeObj);
			spawned = spawned + 1;
		}
	}

	private void SpawnOne(DmDzeObject dzeObj)
	{
		vector pos = Vector(dzeObj.Position[0], dzeObj.Position[1], dzeObj.Position[2]);
		// Vanilla ObjectSpawnerHandler flags, minus pathgraph (no AI here):
		// no CE lifetime, no persistency - arena furniture belongs to us.
		int flags = ECE_SETUP | ECE_CREATEPHYSICS | ECE_NOLIFETIME | ECE_DYNAMIC_PERSISTENCY;
		Object worldObj = GetGame().CreateObjectEx(dzeObj.Type, pos, flags, RF_IGNORE);
		if (!worldObj)
		{
			Print("[DM] arenas: failed to spawn '" + dzeObj.Type + "'");
			return;
		}
		if (dzeObj.Orientation.Count() >= 3)
		{
			worldObj.SetOrientation(Vector(dzeObj.Orientation[0], dzeObj.Orientation[1], dzeObj.Orientation[2]));
		}
		if (dzeObj.Scale != 1.0 && dzeObj.Scale > 0)
		{
			worldObj.SetScale(dzeObj.Scale);
		}
		// DisableSimulation exists on Entity, not Object (boot-proven):
		// static Land_* buildings never simulate anyway, so only dynamic
		// props used as furniture need switching off.
		EntityAI simEntity = EntityAI.Cast(worldObj);
		if (simEntity && !dzeObj.Simulate)
		{
			simEntity.DisableSimulation(true);
		}
		m_ActiveObjects.Insert(worldObj);
	}

	private void DrainDeletes(int maxThisTick)
	{
		if (m_RetiredByZone.Count() == 0) return;
		string retiredZone = m_RetiredByZone.GetKey(0);
		array<Object> retiredSet = m_RetiredByZone.GetElement(0);
		int deleted = 0;
		while (retiredSet.Count() > 0 && deleted < maxThisTick)
		{
			Object worldObj = retiredSet[retiredSet.Count() - 1];
			retiredSet.Remove(retiredSet.Count() - 1);
			if (worldObj) GetGame().ObjectDelete(worldObj);
			deleted = deleted + 1;
		}
		if (retiredSet.Count() == 0)
		{
			m_RetiredByZone.Remove(retiredZone);
			Print("[DM] arenas: retired '" + retiredZone + "' cleared");
		}
	}

	int GetPendingSpawnCount() { return m_SpawnQueue.Count(); }
	int GetActiveObjectCount() { return m_ActiveObjects.Count(); }

	static void SelfTest()
	{
		int arenaOk = 1;
		// Filename policy: plain names only.
		if (!ValidArenaFilename("cqb_pit.dze")) arenaOk = 0;
		if (ValidArenaFilename("")) arenaOk = 0;
		if (ValidArenaFilename("../../serverDZ.cfg")) arenaOk = 0;
		if (ValidArenaFilename("sub\\dir.dze")) arenaOk = 0;
		if (ValidArenaFilename("sub/dir.dze")) arenaOk = 0;
		// Lifecycle bookkeeping on a fresh instance (no world objects).
		DmArenaService probe = new DmArenaService();
		if (probe.GetPendingSpawnCount() != 0) arenaOk = 0;
		if (probe.GetActiveObjectCount() != 0) arenaOk = 0;
		probe.OnArenaChosen("nowhere"); // no set loaded: must be a clean no-op
		if (probe.GetPendingSpawnCount() != 0) arenaOk = 0;
		Print("[DM] fixture DmArenaService policy: expected=1 got=" + arenaOk.ToString() + " " + DmFixture.Verdict(arenaOk == 1));
	}
}
