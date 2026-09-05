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

	// DEPRECATED (v0.1.16): respawn timing is client-driven now (the engine
	// respawn login), so this no longer delays anything. Field kept per the
	// append-only schema rule.
	int RespawnDelaySeconds = 2;
	int SpawnProtectSeconds = 3;
	// Spawn points closer than this to where the player died are avoided on
	// respawn (0 disables). Falls back to all points on tiny arenas.
	int RespawnAvoidDeathMeters = 75;

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

	// Killfeed lines also land in each player's chat: chat scrollback
	// survives the victim's death/respawn blackout, which the 8 s HUD rows
	// do not.
	bool KillfeedToChat = true;

	// Arena rule: going unconscious kills you outright (credited to whoever
	// put you down via the last-attacker memory). Turn off to keep vanilla
	// unconsciousness.
	bool DisableUnconsciousness = true;

	// .dze arena object budgets (DmArenaService). Spawns drain during the
	// countdown into an empty network bubble; deletes drain during the next
	// cycle's vote window.
	int MaxArenaObjects = 1000;
	int MaxArenaSpawnsPerTick = 50;
	int MaxArenaDeletesPerTick = 25;

	// Rotating server announcements (Discord invite, rules, whatever): one
	// line from Announcements lands in every connected player's chat every
	// AnnouncementIntervalSeconds, cycling through the list in order. Empty
	// list or interval 0 = off. The default list is deliberately EMPTY (so
	// an absent key on older files loads as "off", not as "reseed").
	// AnnouncementColor: colorImportant (red) | colorAction (white) |
	// colorFriendly (green) | colorStatusChannel (grey).
	int AnnouncementIntervalSeconds = 300;
	ref array<string> Announcements = new array<string>;
	string AnnouncementColor = "colorImportant";

	// One-shot line sent to each player a few seconds after their FIRST
	// connect of the session (respawns do not repeat it). Empty = off.
	string WelcomeMessage = "";

	// Client chat history: while a player has the chat box open, the last
	// 12 vanilla chat lines stay visible instead of having faded out (they
	// resume fading when the box closes). Off by default so a fresh install
	// is byte-for-byte vanilla chat; the flag travels to clients on join.
	// Compiled out entirely under DayZ Expansion Chat and LBmaster Groups,
	// which ship their own (richer) chat history.
	bool ChatHistoryOnOpen = false;
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
		if (m_Data.RespawnAvoidDeathMeters < 0) m_Data.RespawnAvoidDeathMeters = 0;
		if (m_Data.CorpseLifetimeSeconds < 5) m_Data.CorpseLifetimeSeconds = 5;
		if (m_Data.MaxDeletesPerTick < 1) m_Data.MaxDeletesPerTick = 1;
		if (m_Data.MaxArenaObjects < 1) m_Data.MaxArenaObjects = 1;
		if (m_Data.MaxArenaSpawnsPerTick < 1) m_Data.MaxArenaSpawnsPerTick = 1;
		if (m_Data.MaxArenaDeletesPerTick < 1) m_Data.MaxArenaDeletesPerTick = 1;

		// Announcements: 0 = off, otherwise a 30 s floor so a typo cannot
		// turn chat into a wall of red. Blank/newline-only lines are dropped
		// (chat rows are single-line); embedded newlines are flattened.
		if (m_Data.AnnouncementIntervalSeconds < 0) m_Data.AnnouncementIntervalSeconds = 0;
		if (m_Data.AnnouncementIntervalSeconds > 0 && m_Data.AnnouncementIntervalSeconds < 30) m_Data.AnnouncementIntervalSeconds = 30;
		SanitizeAnnouncements();
		if (!IsKnownChatColor(m_Data.AnnouncementColor)) m_Data.AnnouncementColor = "colorImportant";
		m_Data.WelcomeMessage = FlattenLine(m_Data.WelcomeMessage);
	}

	static bool IsKnownChatColor(string colorClass)
	{
		if (colorClass == "colorImportant") return true;
		if (colorClass == "colorAction") return true;
		if (colorClass == "colorFriendly") return true;
		if (colorClass == "colorStatusChannel") return true;
		return false;
	}

	static string FlattenLine(string text)
	{
		string flat = text;
		flat.Replace("\r", " ");
		flat.Replace("\n", " ");
		return flat.Trim();
	}

	private void SanitizeAnnouncements()
	{
		if (!m_Data.Announcements) m_Data.Announcements = new array<string>;
		ref array<string> kept = new array<string>;
		for (int annIdx = 0; annIdx < m_Data.Announcements.Count(); annIdx++)
		{
			string line = FlattenLine(m_Data.Announcements[annIdx]);
			if (line == "") continue;
			kept.Insert(line);
		}
		m_Data.Announcements = kept;
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
	int GetRespawnAvoidDeathMeters() { return m_Data.RespawnAvoidDeathMeters; }
	int GetCorpseLifetimeSeconds() { return m_Data.CorpseLifetimeSeconds; }
	int GetMaxDeletesPerTick() { return m_Data.MaxDeletesPerTick; }
	string GetDropPolicy() { return m_Data.DropPolicy; }
	bool IsSurvivalPressureDisabled() { return m_Data.DisableSurvivalPressure; }
	bool IsMeleeSpawnEnabled() { return m_Data.MeleeSpawn; }
	bool IsKillfeedToChatEnabled() { return m_Data.KillfeedToChat; }
	bool IsUnconsciousnessDisabled() { return m_Data.DisableUnconsciousness; }
	int GetMaxArenaObjects() { return m_Data.MaxArenaObjects; }
	int GetMaxArenaSpawnsPerTick() { return m_Data.MaxArenaSpawnsPerTick; }
	int GetMaxArenaDeletesPerTick() { return m_Data.MaxArenaDeletesPerTick; }
	int GetAnnouncementIntervalSeconds() { return m_Data.AnnouncementIntervalSeconds; }
	int GetAnnouncementCount() { return m_Data.Announcements.Count(); }
	string GetAnnouncementColor() { return m_Data.AnnouncementColor; }
	string GetWelcomeMessage() { return m_Data.WelcomeMessage; }
	bool IsChatHistoryOnOpenEnabled() { return m_Data.ChatHistoryOnOpen; }

	string GetAnnouncement(int annIdx)
	{
		if (annIdx < 0 || annIdx >= m_Data.Announcements.Count()) return "";
		return m_Data.Announcements[annIdx];
	}

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
		if (!defaults.KillfeedToChat) defOk = 0;
		if (!defaults.DisableUnconsciousness) defOk = 0;
		if (defaults.MaxArenaObjects != 1000) defOk = 0;
		if (defaults.MaxArenaSpawnsPerTick != 50) defOk = 0;
		if (defaults.RespawnAvoidDeathMeters != 75) defOk = 0;
		if (defaults.AnnouncementIntervalSeconds != 300) defOk = 0;
		if (defaults.Announcements.Count() != 0) defOk = 0;
		if (defaults.AnnouncementColor != "colorImportant") defOk = 0;
		if (defaults.WelcomeMessage != "") defOk = 0;
		if (defaults.ChatHistoryOnOpen) defOk = 0;
		Print("[DM] fixture DmConfig defaults: expected=1 got=" + defOk.ToString() + " " + DmFixture.Verdict(defOk == 1));

		DmConfig probe = new DmConfig();
		probe.m_Data = new DmConfigData();
		probe.m_Data.VoteSeconds = 0;
		probe.m_Data.MaxDeletesPerTick = -5;
		probe.m_Data.AnnouncementIntervalSeconds = 5;
		probe.m_Data.AnnouncementColor = "hotpink";
		probe.ClampLoadedValues();
		int clampOk = 1;
		if (probe.m_Data.VoteSeconds != 5) clampOk = 0;
		if (probe.m_Data.MaxDeletesPerTick != 1) clampOk = 0;
		if (probe.m_Data.AnnouncementIntervalSeconds != 30) clampOk = 0;
		if (probe.m_Data.AnnouncementColor != "colorImportant") clampOk = 0;
		Print("[DM] fixture DmConfig clamp floors: expected=1 got=" + clampOk.ToString() + " " + DmFixture.Verdict(clampOk == 1));

		// Announcement sanitizing: blank lines drop, newlines flatten, 0 stays off.
		DmConfig annProbe = new DmConfig();
		annProbe.m_Data = new DmConfigData();
		annProbe.m_Data.AnnouncementIntervalSeconds = 0;
		annProbe.m_Data.Announcements.Insert("  ");
		annProbe.m_Data.Announcements.Insert("Join our\nDiscord");
		annProbe.m_Data.Announcements.Insert("");
		annProbe.m_Data.Announcements.Insert("Second line");
		annProbe.m_Data.WelcomeMessage = "  hi\n ";
		annProbe.ClampLoadedValues();
		int annOk = 1;
		if (annProbe.m_Data.AnnouncementIntervalSeconds != 0) annOk = 0;
		if (annProbe.GetAnnouncementCount() != 2) annOk = 0;
		if (annProbe.GetAnnouncement(0) != "Join our Discord") annOk = 0;
		if (annProbe.GetAnnouncement(1) != "Second line") annOk = 0;
		if (annProbe.GetAnnouncement(2) != "") annOk = 0;
		if (annProbe.GetWelcomeMessage() != "hi") annOk = 0;
		Print("[DM] fixture DmConfig announcement sanitize: expected=1 got=" + annOk.ToString() + " " + DmFixture.Verdict(annOk == 1));
	}
}
