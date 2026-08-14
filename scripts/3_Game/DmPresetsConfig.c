// Weapon preset definitions: $profile:SentinelDeathmatch\presets.json
//
// Classname validation happens in DmLoadoutFactory at boot (4_World has the
// config API); this class owns file I/O and structural validation only.
class DmGearItemData
{
	string ClassName = "";
	int Count = 1;
}

class DmPresetData
{
	string Name = "";
	string IconKey = "";
	bool Enabled = true;

	string PrimaryClass = "";
	ref array<string> PrimaryAttachments = new array<string>;
	// Magazine classname for mag-fed weapons; ammo classname (e.g.
	// "Ammo_12gaPellets") for internal-magazine weapons so the gun spawns
	// loaded with that exact type; "" lets the engine pick a chamberable type.
	string PrimaryMagClass = "";
	int PrimaryMagCount = 3;

	string SecondaryClass = "";
	ref array<string> SecondaryAttachments = new array<string>;
	string SecondaryMagClass = "";
	int SecondaryMagCount = 0;

	ref array<string> Clothing = new array<string>;
	ref array<ref DmGearItemData> Gear = new array<ref DmGearItemData>;
}

// Per-player cosmetic override: this steam64 always spawns in this exact
// clothing list (replaces the preset's Clothing entirely; weapons and gear
// untouched). Validated at boot like everything else.
class DmPlayerCosmeticData
{
	string SteamId = "";
	ref array<string> Clothing = new array<string>;
}

class DmPresetsData
{
	int SchemaVersion = 1;
	ref array<ref DmPresetData> Presets = new array<ref DmPresetData>;
	// Default empty - the array-clearing loader gotcha doesn't apply when
	// empty IS the default.
	ref array<ref DmPlayerCosmeticData> PlayerCosmetics = new array<ref DmPlayerCosmeticData>;
	// Every spawn also gets one random melee weapon from this pool (hotbar
	// slot 4). TESTBED-PROVEN GOTCHA: the JSON loader CLEARS constructor-
	// seeded arrays when the key is absent from an existing file, so Load()
	// reseeds an empty pool with these defaults and persists the field back.
	// Disabling melee is therefore config.json's MeleeSpawn, never [].
	ref array<string> MeleePool = new array<string>;

	void DmPresetsData()
	{
		MeleePool.Insert("CombatKnife");
		MeleePool.Insert("HuntingKnife");
		MeleePool.Insert("KitchenKnife");
		MeleePool.Insert("Machete");
		MeleePool.Insert("Hatchet");
		MeleePool.Insert("BaseballBat");
		MeleePool.Insert("Crowbar");
		MeleePool.Insert("PipeWrench");
	}
}

class DmPresetsConfig
{
	static string PRESETS_PATH = "$profile:SentinelDeathmatch\\presets.json";

	private static ref DmPresetsConfig s_Instance;

	private ref DmPresetsData m_Data;

	static DmPresetsConfig GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new DmPresetsConfig();
			s_Instance.Load();
		}
		return s_Instance;
	}

	void Load()
	{
		m_Data = new DmPresetsData();

		if (FileExist(PRESETS_PATH))
		{
			JsonFileLoader<DmPresetsData>.JsonLoadFile(PRESETS_PATH, m_Data);
			// Files predating MeleePool load it as empty (the loader clears
			// constructor-seeded arrays): reseed and persist the migration.
			if (m_Data.MeleePool.Count() == 0)
			{
				DmPresetsData seed = new DmPresetsData();
				m_Data.MeleePool = seed.MeleePool;
				JsonFileLoader<DmPresetsData>.JsonSaveFile(PRESETS_PATH, m_Data);
				Print("[DM] presets.json: MeleePool missing/empty - reseeded defaults");
			}
		}
		else
		{
			WriteDefaults();
			JsonFileLoader<DmPresetsData>.JsonSaveFile(PRESETS_PATH, m_Data);
		}
	}

	// Vanilla-only default so a fresh install works on any server. Operators
	// replace this file; a bad classname disables the preset at boot with a
	// log line (see DmLoadoutFactory.ValidateAll).
	private void WriteDefaults()
	{
		DmPresetData smg = new DmPresetData();
		smg.Name = "SMG Rush";
		smg.PrimaryClass = "MP5K";
		smg.PrimaryAttachments.Insert("MP5k_StockBttstck");
		smg.PrimaryMagClass = "Mag_MP5_30Rnd";
		smg.PrimaryMagCount = 4;
		smg.Clothing.Insert("TShirt_Blue");
		smg.Clothing.Insert("Jeans_Blue");
		smg.Clothing.Insert("AthleticShoes_Blue");
		smg.Clothing.Insert("CourierBag");
		smg.Clothing.Insert("BallisticHelmet_Black");
		smg.Clothing.Insert("PlateCarrierVest_Black");
		DmGearItemData bandage = new DmGearItemData();
		bandage.ClassName = "BandageDressing";
		bandage.Count = 2;
		smg.Gear.Insert(bandage);
		m_Data.Presets.Insert(smg);

		DmPresetData shotgun = new DmPresetData();
		shotgun.Name = "Shotgun CQB";
		shotgun.PrimaryClass = "Izh43Shotgun";
		shotgun.PrimaryMagClass = "Ammo_12gaPellets";
		shotgun.PrimaryMagCount = 0;
		DmGearItemData shells = new DmGearItemData();
		shells.ClassName = "Ammo_12gaPellets";
		shells.Count = 3;
		shotgun.Gear.Insert(shells);
		shotgun.Clothing.Insert("TShirt_Red");
		shotgun.Clothing.Insert("CanvasPantsMidi_Red");
		shotgun.Clothing.Insert("AthleticShoes_Black");
		shotgun.Clothing.Insert("CourierBag");
		shotgun.Clothing.Insert("BallisticHelmet_Desert");
		shotgun.Clothing.Insert("PlateCarrierVest_Desert");
		m_Data.Presets.Insert(shotgun);
	}

	int GetPresetCount() { return m_Data.Presets.Count(); }

	int GetMeleePoolCount() { return m_Data.MeleePool.Count(); }

	int GetCosmeticCount() { return m_Data.PlayerCosmetics.Count(); }

	DmPlayerCosmeticData GetCosmetic(int cosmeticIdx)
	{
		if (cosmeticIdx < 0 || cosmeticIdx >= m_Data.PlayerCosmetics.Count()) return null;
		return m_Data.PlayerCosmetics[cosmeticIdx];
	}

	string GetMeleePoolEntry(int meleeIdx)
	{
		if (meleeIdx < 0 || meleeIdx >= m_Data.MeleePool.Count()) return "";
		return m_Data.MeleePool[meleeIdx];
	}

	DmPresetData GetPreset(int presetIdx)
	{
		if (presetIdx < 0 || presetIdx >= m_Data.Presets.Count()) return null;
		return m_Data.Presets[presetIdx];
	}

	static void SelfTest()
	{
		DmPresetsConfig probe = new DmPresetsConfig();
		probe.m_Data = new DmPresetsData();
		probe.WriteDefaults();
		int defOk = 1;
		if (probe.GetPresetCount() != 2) defOk = 0;
		DmPresetData firstPreset = probe.GetPreset(0);
		if (!firstPreset) defOk = 0;
		if (firstPreset && firstPreset.PrimaryClass == "") defOk = 0;
		if (probe.GetPreset(99)) defOk = 0;
		// Melee pool ships constructor-seeded and bounds-checked.
		if (probe.GetMeleePoolCount() == 0) defOk = 0;
		if (probe.GetMeleePoolEntry(0) == "") defOk = 0;
		if (probe.GetMeleePoolEntry(99) != "") defOk = 0;
		if (probe.GetMeleePoolEntry(-1) != "") defOk = 0;
		Print("[DM] fixture DmPresetsConfig defaults: expected=1 got=" + defOk.ToString() + " " + DmFixture.Verdict(defOk == 1));
	}
}
