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

	// One demo arena near the Chernarus NWAF so a fresh install boots into a
	// working (if placeholder) rotation. Operators replace this file.
	private void WriteDefaults()
	{
		DmZoneData demo = new DmZoneData();
		demo.Name = "Demo Field";
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
	}

	// Boot validation: a zone that cannot host a round is disabled with a loud
	// log line, never a mid-round failure.
	void Validate()
	{
		m_EnabledIndices.Clear();
		for (int zoneIdx = 0; zoneIdx < m_Data.Zones.Count(); zoneIdx++)
		{
			DmZoneData zone = m_Data.Zones[zoneIdx];
			if (!zone.Enabled) continue;

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
		Print("[DM] zones.json: " + m_EnabledIndices.Count().ToString() + " of " + m_Data.Zones.Count().ToString() + " zones enabled");
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
		probe.Validate();
		int defOk = 1;
		if (probe.GetEnabledCount() != 1) defOk = 0;
		DmZoneData demoZone = probe.GetEnabledZone(0);
		if (!demoZone) defOk = 0;
		if (demoZone && demoZone.SpawnPoints.Count() != 4) defOk = 0;
		Print("[DM] fixture DmZonesConfig defaults: expected=1 got=" + defOk.ToString() + " " + DmFixture.Verdict(defOk == 1));

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
