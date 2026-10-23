// Zone definitions: $profile:SentinelDeathmatch\zones.json
//
// Coordinates are explicit float fields, not vectors: JsonFileLoader's vector
// serialization is not a format we want operators hand-editing. Y (elevation)
// of 0 means "snap to terrain at spawn time".
class DmSpawnPointData
{
	float X = 0;
	float Y = 0;
	float Z = 0;
	float Yaw = 0;
}

class DmZoneData
{
	string Name = "";
	string IconKey = "";
	bool Enabled = true;

	// Which world this zone belongs to: compared case-insensitively against
	// the running server's world name ("chernarusplus", "enoch", "sakhal",
	// "nasdara", ...). Empty = any map. A zone for another map is skipped at
	// boot without a warning - it is that map's arena, not a mistake - so one
	// zones.json can carry every map's arenas and the server picks its own.
	// Added after 0.1.34; older files load with "" and behave as before.
	string Map = "";

	float CenterX = 0;
	float CenterZ = 0;
	float Radius = 200;
	float WarnMargin = 30;

	// "damage"    = soft wall (BR-gas style, scales with overshoot)
	// "teleport"  = snap back to nearest spawn point
	// "countdown" = grace timer outside, then instant death + score penalty
	string Enforcement = "damage";
	float OutOfZoneDmgPerSec = 5.0;
	float OutOfZoneKillSeconds = 10.0;

	// Optional DayZ Editor .dze object set for this arena: a plain filename
	// inside $profile:SentinelDeathmatch\arenas\. Spawned when this zone
	// wins a vote (see DmArenaService).
	string DzeFile = "";

	float LobbyX = 0;
	float LobbyY = 0;
	float LobbyZ = 0;

	ref array<ref DmSpawnPointData> SpawnPoints = new array<ref DmSpawnPointData>;
}

class DmZonesData
{
	int SchemaVersion = 1;
	ref array<ref DmZoneData> Zones = new array<ref DmZoneData>;
}

class DmZonesConfig
{
	static string ZONES_PATH = "$profile:SentinelDeathmatch\\zones.json";

	private static ref DmZonesConfig s_Instance;

	private ref DmZonesData m_Data;
	private ref array<int> m_EnabledIndices = new array<int>;

	static DmZonesConfig GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new DmZonesConfig();
			s_Instance.Load();
		}
		return s_Instance;
	}

	void Load()
	{
		m_Data = new DmZonesData();

		if (FileExist(ZONES_PATH))
		{
			JsonFileLoader<DmZonesData>.JsonLoadFile(ZONES_PATH, m_Data);
		}
		else
		{
			WriteDefaults();
			JsonFileLoader<DmZonesData>.JsonSaveFile(ZONES_PATH, m_Data);
		}

		Validate();
	}

	// One demo arena per shipped map, keyed by Map, so a fresh install boots
	// into a working (if placeholder) rotation on either world: a field near
	// the Chernarus NWAF, and the Ma'bar Ardistan village on Nasdara.
	// Operators replace this file.
	private void WriteDefaults()
	{
		DmZoneData demo = new DmZoneData();
		demo.Name = "Demo Field";
		demo.Map = "chernarusplus";
		demo.CenterX = 4600;
		demo.CenterZ = 10000;
		demo.Radius = 250;
		demo.LobbyX = 4600;
		demo.LobbyZ = 10300;

		float angleStep = 90.0;
		for (int spawnIdx = 0; spawnIdx < 4; spawnIdx++)
		{
			DmSpawnPointData sp = new DmSpawnPointData();
			float angleDeg = spawnIdx * angleStep;
			sp.X = demo.CenterX + Math.Cos(angleDeg * Math.DEG2RAD) * 150;
			sp.Z = demo.CenterZ + Math.Sin(angleDeg * Math.DEG2RAD) * 150;
			sp.Yaw = angleDeg + 180;
			demo.SpawnPoints.Insert(sp);
		}

		m_Data.Zones.Insert(demo);

		// Spawn points are a hand-placed subset of docs/examples/arenas/
		// mabar_ardistan.dze (the full set rides the .dze); a ring around the
		// center would land players inside the village's walls.
		DmZoneData nasdara = new DmZoneData();
		nasdara.Name = "Ma'bar Ardistan";
		nasdara.Map = "nasdara";
		nasdara.CenterX = 3366.5;
		nasdara.CenterZ = 4655.1;
		nasdara.Radius = 81;
		nasdara.WarnMargin = 15;
		nasdara.LobbyX = 3366.5;
		nasdara.LobbyZ = 4760;
		AddSpawn(nasdara, 3363.8, 4593.5, 2.6);
		AddSpawn(nasdara, 3321.8, 4621.5, 53.1);
		AddSpawn(nasdara, 3410.8, 4699.1, 225.2);
		AddSpawn(nasdara, 3410.3, 4648.6, 278.5);
		AddSpawn(nasdara, 3345.8, 4698.6, 154.5);
		AddSpawn(nasdara, 3303.8, 4679.1, 110.9);
		AddSpawn(nasdara, 3420.3, 4609.5, 310.3);
		AddSpawn(nasdara, 3353.8, 4722.1, 169.2);
		m_Data.Zones.Insert(nasdara);
	}

	private static void AddSpawn(DmZoneData zone, float x, float z, float yaw)
	{
		DmSpawnPointData sp = new DmSpawnPointData();
		sp.X = x;
		sp.Z = z;
		sp.Yaw = yaw;
		zone.SpawnPoints.Insert(sp);
	}

	// The running world, lowercased ("chernarusplus", "nasdara", ...). Empty
	// when no game exists (fixtures): callers then skip map filtering.
	static string CurrentWorldName()
	{
		string worldName = "";
		if (GetGame()) GetGame().GetWorldName(worldName);
		return NormalizeMapName(worldName);
	}

	static string NormalizeMapName(string mapName)
	{
		string lowered = mapName;
		lowered = lowered.Trim();
		lowered.ToLower();
		return lowered;
	}

	// Map association rule, shared with the .dze loader: an empty zone map
	// means "any world", an empty world name means "unknown, do not filter".
	// Pure; fixtures.
	static bool ZoneMatchesWorld(string zoneMap, string worldName)
	{
		string zoneMapNorm = NormalizeMapName(zoneMap);
		if (zoneMapNorm == "") return true;
		string worldNorm = NormalizeMapName(worldName);
		if (worldNorm == "") return true;
		return zoneMapNorm == worldNorm;
	}

	// Boot validation: a zone that cannot host a round is disabled with a loud
	// log line, never a mid-round failure.
	void Validate()
	{
		ValidateForWorld(CurrentWorldName());
	}

	void ValidateForWorld(string worldName)
	{
		m_EnabledIndices.Clear();
		int otherMapCount = 0;
		for (int zoneIdx = 0; zoneIdx < m_Data.Zones.Count(); zoneIdx++)
		{
			DmZoneData zone = m_Data.Zones[zoneIdx];
			if (!zone.Enabled) continue;
			if (!ZoneMatchesWorld(zone.Map, worldName))
			{
				otherMapCount = otherMapCount + 1;
				continue;
			}

			if (zone.Name == "")
			{
				Print("[DM] zones.json: zone " + zoneIdx.ToString() + " has no Name - disabled");
				continue;
			}
			if (zone.Radius < 50)
			{
				Print("[DM] zones.json: zone '" + zone.Name + "' Radius < 50 m - disabled");
				continue;
			}
			if (zone.SpawnPoints.Count() < 2)
			{
				Print("[DM] zones.json: zone '" + zone.Name + "' needs >= 2 SpawnPoints - disabled");
				continue;
			}
			if (zone.Enforcement != "damage" && zone.Enforcement != "teleport" && zone.Enforcement != "countdown")
			{
				Print("[DM] zones.json: zone '" + zone.Name + "' unknown Enforcement '" + zone.Enforcement + "' - using 'damage'");
				zone.Enforcement = "damage";
			}
			if (zone.OutOfZoneKillSeconds < 3.0) zone.OutOfZoneKillSeconds = 3.0;
			m_EnabledIndices.Insert(zoneIdx);
		}
		string summary = "[DM] zones.json: " + m_EnabledIndices.Count().ToString() + " of " + m_Data.Zones.Count().ToString() + " zones enabled";
		if (otherMapCount > 0)
		{
			summary = summary + " (" + otherMapCount.ToString() + " for other maps skipped on '" + worldName + "')";
		}
		Print(summary);
	}

	// Raw access (all zones, pre-validation) - the arena loader applies .dze
	// marker geometry to raw zones and then re-runs Validate().
	int GetZoneCount() { return m_Data.Zones.Count(); }

	DmZoneData GetZone(int zoneIdx)
	{
		if (zoneIdx < 0 || zoneIdx >= m_Data.Zones.Count()) return null;
		return m_Data.Zones[zoneIdx];
	}

	int GetEnabledCount() { return m_EnabledIndices.Count(); }

	// Post-load disable (e.g. an arena file that can't materialize): pulls
	// the zone from the enabled list so votes can never land on it.
	void DisableZoneByName(string zoneName)
	{
		for (int enabledIdx = 0; enabledIdx < m_EnabledIndices.Count(); enabledIdx++)
		{
			DmZoneData zone = m_Data.Zones[m_EnabledIndices[enabledIdx]];
			if (zone && zone.Name == zoneName)
			{
				m_EnabledIndices.RemoveOrdered(enabledIdx);
				Print("[DM] zones.json: zone '" + zoneName + "' disabled post-load");
				return;
			}
		}
	}

	DmZoneData GetEnabledZone(int enabledIdx)
	{
		if (enabledIdx < 0 || enabledIdx >= m_EnabledIndices.Count()) return null;
		return m_Data.Zones[m_EnabledIndices[enabledIdx]];
	}

	static void SelfTest()
	{
		DmZonesConfig probe = new DmZonesConfig();
		probe.m_Data = new DmZonesData();
		probe.WriteDefaults();
		probe.ValidateForWorld("chernarusplus");
		int defOk = 1;
		if (probe.GetZoneCount() != 2) defOk = 0;
		if (probe.GetEnabledCount() != 1) defOk = 0;
		DmZoneData demoZone = probe.GetEnabledZone(0);
		if (!demoZone) defOk = 0;
		if (demoZone && demoZone.Name != "Demo Field") defOk = 0;
		if (demoZone && demoZone.SpawnPoints.Count() != 4) defOk = 0;
		probe.ValidateForWorld("nasdara");
		if (probe.GetEnabledCount() != 1) defOk = 0;
		DmZoneData nasdaraZone = probe.GetEnabledZone(0);
		if (!nasdaraZone) defOk = 0;
		if (nasdaraZone && nasdaraZone.Map != "nasdara") defOk = 0;
		if (nasdaraZone && nasdaraZone.SpawnPoints.Count() < 2) defOk = 0;
		if (nasdaraZone && nasdaraZone.Radius < 50) defOk = 0;
		Print("[DM] fixture DmZonesConfig defaults: expected=1 got=" + defOk.ToString() + " " + DmFixture.Verdict(defOk == 1));

		// Map association: empty zone map = any world, empty world = no
		// filtering, otherwise a case-insensitive match.
		int mapOk = 1;
		if (!ZoneMatchesWorld("", "chernarusplus")) mapOk = 0;
		if (!ZoneMatchesWorld("ChernarusPlus", "chernarusplus")) mapOk = 0;
		if (!ZoneMatchesWorld(" nasdara ", "NASDARA")) mapOk = 0;
		if (!ZoneMatchesWorld("nasdara", "")) mapOk = 0;
		if (ZoneMatchesWorld("nasdara", "chernarusplus")) mapOk = 0;
		probe.ValidateForWorld("");
		if (probe.GetEnabledCount() != 2) mapOk = 0;
		probe.ValidateForWorld("enoch");
		if (probe.GetEnabledCount() != 0) mapOk = 0;
		Print("[DM] fixture DmZonesConfig map filter: expected=1 got=" + mapOk.ToString() + " " + DmFixture.Verdict(mapOk == 1));

		DmZonesConfig probeBad = new DmZonesConfig();
		probeBad.m_Data = new DmZonesData();
		DmZoneData badZone = new DmZoneData();
		badZone.Name = "TooSmall";
		badZone.Radius = 10;
		probeBad.m_Data.Zones.Insert(badZone);
		probeBad.Validate();
		int rejectOk = 1;
		if (probeBad.GetEnabledCount() != 0) rejectOk = 0;
		Print("[DM] fixture DmZonesConfig rejects invalid: expected=1 got=" + rejectOk.ToString() + " " + DmFixture.Verdict(rejectOk == 1));
	}
}
