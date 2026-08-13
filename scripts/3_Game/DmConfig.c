// Server-side JSON config: $profile:SentinelDeathmatch\config.json
//
// Pattern rules (learned the hard way in this mod family):
//  - Defaults live as field initializers on the data class. JsonFileLoader
//    tolerates missing fields (new fields are additive) but NEVER overwrites
//    a field that already exists in the file - so never rename or repurpose
//    a field; add a new one and keep the old with a DEPRECATED comment.
//  - First boot writes the defaults out so operators always have a complete
//    file to edit.
//  - Hot-path accessors must not do per-call string work; cache anything
//    the round engine reads every tick.
class DmConfigData
{
	bool Enabled = true;
	bool DebugLog = false;

	int MinPlayers = 2;
	int VoteSeconds = 30;
	// Consensus fast-forward: once a strict majority of connected players
	// votes for the SAME zone+preset combo, the remaining vote window clamps
	// to this many seconds (only ever shortens). Set >= VoteSeconds to
	// effectively disable.
	int VoteConsensusSeconds = 10;
	int CountdownSeconds = 10;
	int RoundSeconds = 600;
	int ScoreboardSeconds = 15;
	int ScoreLimit = 30;

	int RespawnDelaySeconds = 2;
	int SpawnProtectSeconds = 3;

	int CorpseLifetimeSeconds = 45;
	int MaxDeletesPerTick = 3;
	string DropPolicy = "allow"; // "allow" | "block"

	// Deathmatch is not a survival game: periodically top up water/energy,
	// neutralize heat comfort, and refill stamina for every player.
	bool DisableSurvivalPressure = true;

	// Every spawn gets one random melee weapon from presets.json's MeleePool
	// (hotbar slot 4). The on/off switch lives here because an empty
	// MeleePool array cannot mean "disabled": the JSON loader clears
	// constructor-seeded arrays for files predating the field, so empty
	// means "reseed defaults", not "off".
	bool MeleeSpawn = true;
}

class DmConfig
{
	static string CONFIG_DIR  = "$profile:SentinelDeathmatch";
	static string CONFIG_PATH = "$profile:SentinelDeathmatch\\config.json";

	private static ref DmConfig s_Instance;

	private ref DmConfigData m_Data;
	private bool m_CachedEnabled;

	static DmConfig GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new DmConfig();
			s_Instance.Load();
		}
		return s_Instance;
	}

	void Load()
	{
		m_Data = new DmConfigData();

		if (!FileExist(CONFIG_DIR))
		{
			MakeDirectory(CONFIG_DIR);
		}

		if (FileExist(CONFIG_PATH))
		{
			JsonFileLoader<DmConfigData>.JsonLoadFile(CONFIG_PATH, m_Data);
		}
		else
		{
			Save();
		}

		RefreshCachedFlags();
		ClampLoadedValues();
	}

	void Save()
	{
		JsonFileLoader<DmConfigData>.JsonSaveFile(CONFIG_PATH, m_Data);
	}

	// Materialized because the round engine checks this every tick.
	void RefreshCachedFlags()
	{
		m_CachedEnabled = m_Data.Enabled;
	}

	// Defensive floors so a hand-edited file cannot produce a broken loop
	// (e.g. VoteSeconds 0 would skip voting entirely and spin phases).
	void ClampLoadedValues()
	{
		if (m_Data.MinPlayers < 1) m_Data.MinPlayers = 1;
		if (m_Data.VoteSeconds < 5) m_Data.VoteSeconds = 5;
		if (m_Data.VoteConsensusSeconds < 3) m_Data.VoteConsensusSeconds = 3;
		if (m_Data.CountdownSeconds < 3) m_Data.CountdownSeconds = 3;
		if (m_Data.RoundSeconds < 60) m_Data.RoundSeconds = 60;
		if (m_Data.ScoreboardSeconds < 5) m_Data.ScoreboardSeconds = 5;
		if (m_Data.ScoreLimit < 1) m_Data.ScoreLimit = 1;
		if (m_Data.RespawnDelaySeconds < 0) m_Data.RespawnDelaySeconds = 0;
		if (m_Data.SpawnProtectSeconds < 0) m_Data.SpawnProtectSeconds = 0;
		if (m_Data.CorpseLifetimeSeconds < 5) m_Data.CorpseLifetimeSeconds = 5;
		if (m_Data.MaxDeletesPerTick < 1) m_Data.MaxDeletesPerTick = 1;
	}

	bool IsEnabled() { return m_CachedEnabled; }
	bool IsDebug() { return m_Data.DebugLog; }

	int GetMinPlayers() { return m_Data.MinPlayers; }
	int GetVoteSeconds() { return m_Data.VoteSeconds; }
	int GetVoteConsensusSeconds() { return m_Data.VoteConsensusSeconds; }
	int GetCountdownSeconds() { return m_Data.CountdownSeconds; }
	int GetRoundSeconds() { return m_Data.RoundSeconds; }
	int GetScoreboardSeconds() { return m_Data.ScoreboardSeconds; }
	int GetScoreLimit() { return m_Data.ScoreLimit; }
	int GetRespawnDelaySeconds() { return m_Data.RespawnDelaySeconds; }
	int GetSpawnProtectSeconds() { return m_Data.SpawnProtectSeconds; }
	int GetCorpseLifetimeSeconds() { return m_Data.CorpseLifetimeSeconds; }
	int GetMaxDeletesPerTick() { return m_Data.MaxDeletesPerTick; }
	string GetDropPolicy() { return m_Data.DropPolicy; }
	bool IsSurvivalPressureDisabled() { return m_Data.DisableSurvivalPressure; }
	bool IsMeleeSpawnEnabled() { return m_Data.MeleeSpawn; }

	static void SelfTest()
	{
		DmConfigData defaults = new DmConfigData();
		int defOk = 1;
		if (!defaults.Enabled) defOk = 0;
		if (defaults.MinPlayers != 2) defOk = 0;
		if (defaults.RoundSeconds != 600) defOk = 0;
		if (defaults.DropPolicy != "allow") defOk = 0;
		if (!defaults.MeleeSpawn) defOk = 0;
		if (defaults.VoteConsensusSeconds != 10) defOk = 0;
		Print("[DM] fixture DmConfig defaults: expected=1 got=" + defOk.ToString() + " " + DmFixture.Verdict(defOk == 1));

		DmConfig probe = new DmConfig();
		probe.m_Data = new DmConfigData();
		probe.m_Data.VoteSeconds = 0;
		probe.m_Data.MaxDeletesPerTick = -5;
		probe.ClampLoadedValues();
		int clampOk = 1;
		if (probe.m_Data.VoteSeconds != 5) clampOk = 0;
		if (probe.m_Data.MaxDeletesPerTick != 1) clampOk = 0;
		Print("[DM] fixture DmConfig clamp floors: expected=1 got=" + clampOk.ToString() + " " + DmFixture.Verdict(clampOk == 1));
	}
}
