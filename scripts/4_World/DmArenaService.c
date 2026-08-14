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

	// Called from the round engine's Start after zones.json loads. Works on
	// the RAW zone list because dm: markers may supply the very geometry
	// (spawns, radius) that validation demands; zones are re-validated after
	// marker application. An arena problem disables its ZONE (loud, like a
	// bad preset) so a vote can never land on an arena that cannot
	// materialize.
	void LoadAll()
	{
		m_SetsByZone.Clear();
		DmZonesConfig zones = DmZonesConfig.GetInstance();

		int totalObjects = 0;
		for (int zoneIdx = 0; zoneIdx < zones.GetZoneCount(); zoneIdx++)
		{
			DmZoneData zone = zones.GetZone(zoneIdx);
			if (!zone || !zone.Enabled || zone.DzeFile == "") continue;
			string zoneName = zone.Name;
			string dzeFile = zone.DzeFile;

			if (!ValidArenaFilename(dzeFile))
			{
				Print("[DM] arenas: zone '" + zoneName + "' DzeFile '" + dzeFile + "' is not a plain filename - zone disabled");
				zone.Enabled = false;
				continue;
			}
			string dzePath = ARENAS_DIR + "\\" + dzeFile;
			if (!FileExist(dzePath))
			{
				Print("[DM] arenas: zone '" + zoneName + "' file not found: " + dzePath + " - zone disabled");
				zone.Enabled = false;
				continue;
			}

			DmDzeSave save = new DmDzeSave();
			JsonFileLoader<DmDzeSave>.JsonLoadFile(dzePath, save);
			if (save.EditorObjects.Count() == 0)
			{
				Print("[DM] arenas: zone '" + zoneName + "' " + dzeFile + " has no objects (binary .dze? re-save as JSON in the Editor) - zone disabled");
				zone.Enabled = false;
				continue;
			}

			// dm: markers are consumed as geometry, never spawned.
			array<ref DmDzeObject> markers = new array<ref DmDzeObject>;
			array<ref DmDzeObject> spawnable = new array<ref DmDzeObject>;
			string badClass = CollectSpawnable(save.EditorObjects, spawnable, markers);
			if (badClass != "")
			{
				Print("[DM] arenas: zone '" + zoneName + "' references unknown classname '" + badClass + "' (mod not loaded?) - zone disabled");
				zone.Enabled = false;
				continue;
			}
			if (spawnable.Count() > DmConfig.GetInstance().GetMaxArenaObjects())
			{
				Print("[DM] arenas: zone '" + zoneName + "' has " + spawnable.Count().ToString() + " objects (MaxArenaObjects " + DmConfig.GetInstance().GetMaxArenaObjects().ToString() + ") - zone disabled");
				zone.Enabled = false;
				continue;
			}
			if (spawnable.Count() > WARN_OBJECTS)
			{
				Print("[DM] arenas: zone '" + zoneName + "' has " + spawnable.Count().ToString() + " objects - heavy; consider trimming");
			}

			if (markers.Count() > 0)
			{
				string markerSummary = ApplyMarkers(zone, markers);
				Print("[DM] arenas: zone '" + zoneName + "' geometry from markers: " + markerSummary);
			}

			m_SetsByZone.Set(zoneName, spawnable);
			totalObjects = totalObjects + spawnable.Count();
			Print("[DM] arenas: zone '" + zoneName + "' <- " + dzeFile + " (" + spawnable.Count().ToString() + " objects)");
		}

		// Marker geometry may have made previously-invalid zones valid (and
		// arena failures the reverse): one authoritative re-validation.
		zones.Validate();
		Print("[DM] arenas: " + m_SetsByZone.Count().ToString() + " arena object sets loaded, " + totalObjects.ToString() + " objects total");
	}

	// "dm:xxx" (case-insensitive, trimmed) DisplayNames are directives:
	// center/edge/spawn/lobby. Returns "" for ordinary scenery. Pure; fixtures.
	static string MarkerKind(string displayName)
	{
		string lowered = displayName;
		lowered = lowered.Trim();
		lowered.ToLower();
		if (lowered.IndexOf("dm:") != 0) return "";
		return lowered.Substring(3, lowered.Length() - 3);
	}

	// Derive zone geometry from markers. dm:center moves the circle; dm:edge
	// markers size it (farthest from center wins, clamped to [50, 300] - the
	// single-bubble streaming rule); any dm:spawn markers REPLACE the JSON
	// spawn points (marker yaw = spawn facing, marker Y kept exactly so
	// rooftop spawns work); dm:lobby moves the lobby. Anything not provided
	// keeps its zones.json value. Returns a log summary. Pure; fixtures.
	static string ApplyMarkers(DmZoneData zone, array<ref DmDzeObject> markers)
	{
		int edgeCount = 0;
		int spawnCount = 0;
		ref array<ref DmSpawnPointData> markerSpawns = new array<ref DmSpawnPointData>;

		// Center first: edge distances measure from the final center.
		for (int centerIdx = 0; centerIdx < markers.Count(); centerIdx++)
		{
			DmDzeObject centerMarker = markers[centerIdx];
			if (MarkerKind(centerMarker.DisplayName) != "center") continue;
			if (centerMarker.Position.Count() < 3) continue;
			zone.CenterX = centerMarker.Position[0];
			zone.CenterZ = centerMarker.Position[2];
			break;
		}

		float maxEdgeDist = 0;
		for (int markerIdx = 0; markerIdx < markers.Count(); markerIdx++)
		{
			DmDzeObject marker = markers[markerIdx];
			if (marker.Position.Count() < 3) continue;
			string kind = MarkerKind(marker.DisplayName);
			if (kind == "edge")
			{
				edgeCount = edgeCount + 1;
				float edx = marker.Position[0] - zone.CenterX;
				float edz = marker.Position[2] - zone.CenterZ;
				float edgeDist = Math.Sqrt(edx * edx + edz * edz);
				if (edgeDist > maxEdgeDist) maxEdgeDist = edgeDist;
			}
			else if (kind == "spawn")
			{
				spawnCount = spawnCount + 1;
				DmSpawnPointData sp = new DmSpawnPointData();
				sp.X = marker.Position[0];
				sp.Y = marker.Position[1];
				sp.Z = marker.Position[2];
				if (marker.Orientation.Count() >= 1) sp.Yaw = marker.Orientation[0];
				markerSpawns.Insert(sp);
			}
			else if (kind == "lobby")
			{
				zone.LobbyX = marker.Position[0];
				zone.LobbyY = marker.Position[1];
				zone.LobbyZ = marker.Position[2];
			}
		}

		if (edgeCount > 0)
		{
			float derivedRadius = maxEdgeDist;
			if (derivedRadius < 50) derivedRadius = 50;
			if (derivedRadius > 300) derivedRadius = 300;
			zone.Radius = derivedRadius;
		}
		if (markerSpawns.Count() > 0)
		{
			zone.SpawnPoints = markerSpawns;
		}

		return "center=(" + zone.CenterX.ToString() + ", " + zone.CenterZ.ToString() + ") radius=" + zone.Radius.ToString() + " (" + edgeCount.ToString() + " edge markers) spawns=" + spawnCount.ToString();
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

	// Split raw objects into spawnable scenery and dm: geometry markers;
	// returns the first unresolvable classname, or "" when clean. EditorOnly
	// entries and empty types are skipped; .p3d path placements are skipped
	// with a log (classname objects first). Markers are exempt from classname
	// validation: the object a creator names dm:edge is just their visual
	// handle in the Editor and never spawns.
	private string CollectSpawnable(array<ref DmDzeObject> rawObjects, array<ref DmDzeObject> outSpawnable, array<ref DmDzeObject> outMarkers)
	{
		for (int objIdx = 0; objIdx < rawObjects.Count(); objIdx++)
		{
			DmDzeObject dzeObj = rawObjects[objIdx];
			if (!dzeObj || dzeObj.Type == "") continue;
			if (MarkerKind(dzeObj.DisplayName) != "")
			{
				outMarkers.Insert(dzeObj);
				continue;
			}
			if (dzeObj.EditorOnly) continue;
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

		// dm: marker parsing and geometry derivation.
		int markerOk = 1;
		if (MarkerKind("dm:center") != "center") markerOk = 0;
		if (MarkerKind(" DM:Edge ") != "edge") markerOk = 0;
		if (MarkerKind("container cover A") != "") markerOk = 0;
		if (MarkerKind("") != "") markerOk = 0;

		DmZoneData markerZone = new DmZoneData();
		markerZone.CenterX = 0;
		markerZone.CenterZ = 0;
		markerZone.Radius = 200;
		array<ref DmDzeObject> testMarkers = new array<ref DmDzeObject>;
		DmDzeObject centerM = new DmDzeObject();
		centerM.DisplayName = "dm:center";
		centerM.Position.Insert(1000); centerM.Position.Insert(10); centerM.Position.Insert(2000);
		testMarkers.Insert(centerM);
		DmDzeObject edgeNear = new DmDzeObject();
		edgeNear.DisplayName = "dm:edge";
		edgeNear.Position.Insert(1100); edgeNear.Position.Insert(0); edgeNear.Position.Insert(2000);
		testMarkers.Insert(edgeNear);
		DmDzeObject edgeFar = new DmDzeObject();
		edgeFar.DisplayName = "dm:edge";
		edgeFar.Position.Insert(1000); edgeFar.Position.Insert(0); edgeFar.Position.Insert(2220);
		testMarkers.Insert(edgeFar);
		DmDzeObject spawnM = new DmDzeObject();
		spawnM.DisplayName = "dm:spawn";
		spawnM.Position.Insert(1050); spawnM.Position.Insert(12.5); spawnM.Position.Insert(2050);
		spawnM.Orientation.Insert(90);
		testMarkers.Insert(spawnM);
		ApplyMarkers(markerZone, testMarkers);
		if (markerZone.CenterX != 1000 || markerZone.CenterZ != 2000) markerOk = 0;
		if (markerZone.Radius != 220) markerOk = 0; // farthest edge wins
		if (markerZone.SpawnPoints.Count() != 1) markerOk = 0;
		if (markerZone.SpawnPoints[0].Yaw != 90) markerOk = 0;
		if (markerZone.SpawnPoints[0].Y != 12.5) markerOk = 0;

		// Radius clamps to the single-bubble ceiling.
		DmZoneData clampZone = new DmZoneData();
		clampZone.CenterX = 0;
		clampZone.CenterZ = 0;
		array<ref DmDzeObject> clampMarkers = new array<ref DmDzeObject>;
		DmDzeObject edgeHuge = new DmDzeObject();
		edgeHuge.DisplayName = "dm:edge";
		edgeHuge.Position.Insert(5000); edgeHuge.Position.Insert(0); edgeHuge.Position.Insert(0);
		clampMarkers.Insert(edgeHuge);
		ApplyMarkers(clampZone, clampMarkers);
		if (clampZone.Radius != 300) markerOk = 0;
		Print("[DM] fixture DmArenaService markers: expected=1 got=" + markerOk.ToString() + " " + DmFixture.Verdict(markerOk == 1));
	}
}
