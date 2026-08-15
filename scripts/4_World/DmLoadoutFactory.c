// Preset validation and staged loadout injection.
//
// Injection is deliberately staged across frames via the CallQueue: clearing
// and refilling an inventory in the same frame is unreliable. Stage 1 clears,
// stage 2 dresses, stage 3 arms.
class DmLoadoutFactory
{
	private static ref DmLoadoutFactory s_Instance;

	private ref array<int> m_ValidPresetIndices = new array<int>;
	private ref array<string> m_ValidMeleeClasses = new array<string>;
	// raw meta-preset index -> raw member preset indices.
	private ref map<int, ref array<int>> m_MetaMembers = new map<int, ref array<int>>;
	// steam64 -> validated full clothing replacement.
	private ref map<string, ref array<string>> m_ValidCosmetics = new map<string, ref array<string>>;

	static DmLoadoutFactory GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new DmLoadoutFactory();
		}
		return s_Instance;
	}

	// Boot-time: every classname in every preset checked against the config
	// tree once. A bad entry disables the preset with a loud log line so a
	// typo can never throw mid-round.
	void ValidateAll()
	{
		m_ValidPresetIndices.Clear();
		m_MetaMembers.Clear();
		DmPresetsConfig presets = DmPresetsConfig.GetInstance();

		// Pass 1: ordinary presets.
		for (int presetIdx = 0; presetIdx < presets.GetPresetCount(); presetIdx++)
		{
			DmPresetData preset = presets.GetPreset(presetIdx);
			if (!preset.Enabled) continue;
			if (preset.RandomFrom.Count() > 0) continue; // meta: pass 2

			string badClass = FindInvalidClassname(preset);
			if (badClass != "")
			{
				Print("[DM] presets.json: preset '" + preset.Name + "' references unknown classname '" + badClass + "' - disabled");
				continue;
			}
			if (preset.Name == "")
			{
				Print("[DM] presets.json: preset " + presetIdx.ToString() + " has no Name - disabled");
				continue;
			}
			m_ValidPresetIndices.Insert(presetIdx);
		}

		// Pass 2: meta-presets resolve against the pass-1 survivors.
		for (int metaIdx = 0; metaIdx < presets.GetPresetCount(); metaIdx++)
		{
			DmPresetData metaPreset = presets.GetPreset(metaIdx);
			if (!metaPreset.Enabled || metaPreset.RandomFrom.Count() == 0) continue;
			if (metaPreset.Name == "")
			{
				Print("[DM] presets.json: meta-preset " + metaIdx.ToString() + " has no Name - disabled");
				continue;
			}
			array<int> memberIndices = ResolveMetaMembers(presets, m_ValidPresetIndices, metaPreset);
			if (memberIndices.Count() == 0)
			{
				Print("[DM] presets.json: meta-preset '" + metaPreset.Name + "' has no valid members - disabled");
				continue;
			}
			m_MetaMembers.Set(metaIdx, memberIndices);
			m_ValidPresetIndices.Insert(metaIdx);
			Print("[DM] presets.json: meta-preset '" + metaPreset.Name + "' -> " + memberIndices.Count().ToString() + " member loadouts");
		}
		Print("[DM] presets.json: " + m_ValidPresetIndices.Count().ToString() + " of " + presets.GetPresetCount().ToString() + " presets valid");

		m_ValidMeleeClasses.Clear();
		for (int meleeIdx = 0; meleeIdx < presets.GetMeleePoolCount(); meleeIdx++)
		{
			string meleeClass = presets.GetMeleePoolEntry(meleeIdx);
			if (meleeClass == "" || !ClassExists(meleeClass))
			{
				Print("[DM] presets.json: MeleePool entry '" + meleeClass + "' unknown - skipped");
				continue;
			}
			m_ValidMeleeClasses.Insert(meleeClass);
		}
		Print("[DM] presets.json: " + m_ValidMeleeClasses.Count().ToString() + " of " + presets.GetMeleePoolCount().ToString() + " melee pool entries valid");

		m_ValidCosmetics.Clear();
		for (int cosIdx = 0; cosIdx < presets.GetCosmeticCount(); cosIdx++)
		{
			DmPlayerCosmeticData cosmetic = presets.GetCosmetic(cosIdx);
			if (!cosmetic || cosmetic.SteamId == "" || cosmetic.Clothing.Count() == 0) continue;
			bool cosmeticOk = true;
			for (int ccIdx = 0; ccIdx < cosmetic.Clothing.Count(); ccIdx++)
			{
				if (!ClassExists(cosmetic.Clothing[ccIdx]))
				{
					Print("[DM] presets.json: PlayerCosmetics for " + cosmetic.SteamId + " references unknown classname '" + cosmetic.Clothing[ccIdx] + "' - entry disabled");
					cosmeticOk = false;
					break;
				}
			}
			if (cosmeticOk) m_ValidCosmetics.Set(cosmetic.SteamId, cosmetic.Clothing);
		}
		if (presets.GetCosmeticCount() > 0)
		{
			Print("[DM] presets.json: " + m_ValidCosmetics.Count().ToString() + " of " + presets.GetCosmeticCount().ToString() + " player cosmetics valid");
		}
	}

	// The clothing list this player actually spawns in: their cosmetic
	// override when one exists, else the preset's list.
	array<string> ResolveClothing(PlayerBase pb, DmPresetData preset)
	{
		PlayerIdentity ident = pb.GetIdentity();
		if (ident)
		{
			array<string> cosmeticClothing;
			if (m_ValidCosmetics.Find(ident.GetPlainId(), cosmeticClothing)) return cosmeticClothing;
		}
		return preset.Clothing;
	}

	// Returns the first unknown classname, or "" when all resolve. Static
	// checks split out so the walk order is obvious and testable by reading.
	private string FindInvalidClassname(DmPresetData preset)
	{
		if (preset.PrimaryClass != "" && !ClassExists(preset.PrimaryClass)) return preset.PrimaryClass;
		if (preset.PrimaryMagClass != "" && !ClassExists(preset.PrimaryMagClass)) return preset.PrimaryMagClass;
		if (preset.SecondaryClass != "" && !ClassExists(preset.SecondaryClass)) return preset.SecondaryClass;
		if (preset.SecondaryMagClass != "" && !ClassExists(preset.SecondaryMagClass)) return preset.SecondaryMagClass;

		for (int attIdx = 0; attIdx < preset.PrimaryAttachments.Count(); attIdx++)
		{
			if (!ClassExists(preset.PrimaryAttachments[attIdx])) return preset.PrimaryAttachments[attIdx];
		}
		for (int satIdx = 0; satIdx < preset.SecondaryAttachments.Count(); satIdx++)
		{
			if (!ClassExists(preset.SecondaryAttachments[satIdx])) return preset.SecondaryAttachments[satIdx];
		}
		for (int clothIdx = 0; clothIdx < preset.Clothing.Count(); clothIdx++)
		{
			if (!ClassExists(preset.Clothing[clothIdx])) return preset.Clothing[clothIdx];
		}
		for (int gearIdx = 0; gearIdx < preset.Gear.Count(); gearIdx++)
		{
			if (!ClassExists(preset.Gear[gearIdx].ClassName)) return preset.Gear[gearIdx].ClassName;
		}
		return "";
	}

	private bool ClassExists(string className)
	{
		// One test per line: a multi-line || chain trips the line-continuation
		// parser rule (statements end at end-of-line when syntactically complete).
		if (GetGame().ConfigIsExisting("CfgVehicles " + className)) return true;
		if (GetGame().ConfigIsExisting("CfgWeapons " + className)) return true;
		if (GetGame().ConfigIsExisting("CfgMagazines " + className)) return true;
		if (GetGame().ConfigIsExisting("CfgAmmo " + className)) return true;
		return false;
	}

	int GetValidPresetCount() { return m_ValidPresetIndices.Count(); }

	int GetValidMeleeCount() { return m_ValidMeleeClasses.Count(); }

	string GetValidMeleeClass(int meleeIdx)
	{
		if (meleeIdx < 0 || meleeIdx >= m_ValidMeleeClasses.Count()) return "";
		return m_ValidMeleeClasses[meleeIdx];
	}

	DmPresetData GetValidPreset(int validIdx)
	{
		if (validIdx < 0 || validIdx >= m_ValidPresetIndices.Count()) return null;
		return DmPresetsConfig.GetInstance().GetPreset(m_ValidPresetIndices[validIdx]);
	}

	// Membership by NAME against the pass-1 valid raw indices; a meta can
	// never contain itself or another meta. Pure; fixtures.
	static array<int> ResolveMetaMembers(DmPresetsConfig presets, array<int> validRawIndices, DmPresetData metaPreset)
	{
		array<int> memberIndices = new array<int>;
		for (int nameIdx = 0; nameIdx < metaPreset.RandomFrom.Count(); nameIdx++)
		{
			string memberName = metaPreset.RandomFrom[nameIdx];
			for (int validIdx = 0; validIdx < validRawIndices.Count(); validIdx++)
			{
				DmPresetData candidate = presets.GetPreset(validRawIndices[validIdx]);
				if (candidate && candidate.Name == memberName && candidate.RandomFrom.Count() == 0)
				{
					memberIndices.Insert(validRawIndices[validIdx]);
					break;
				}
			}
		}
		return memberIndices;
	}

	// Apply a validated preset (by valid-index). A meta-preset rolls one of
	// its members HERE - once per spawn - and the concrete raw index rides
	// through the staging chain. Stage 1 runs next frame.
	void Apply(PlayerBase pb, int validIdx)
	{
		if (!pb) return;
		if (validIdx < 0 || validIdx >= m_ValidPresetIndices.Count()) return;
		int rawIdx = m_ValidPresetIndices[validIdx];
		array<int> memberIndices;
		if (m_MetaMembers.Find(rawIdx, memberIndices) && memberIndices.Count() > 0)
		{
			rawIdx = memberIndices[Math.RandomInt(0, memberIndices.Count())];
		}
		if (!DmPresetsConfig.GetInstance().GetPreset(rawIdx)) return;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(StageClear, 25, false, pb, rawIdx);
	}

	void StageClear(PlayerBase pb, int rawPresetIdx)
	{
		if (!pb || !pb.IsAlive()) return;
		pb.ClearInventory();
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(StageClothing, 50, false, pb, rawPresetIdx);
	}

	void StageClothing(PlayerBase pb, int rawPresetIdx)
	{
		if (!pb || !pb.IsAlive()) return;
		DmPresetData preset = DmPresetsConfig.GetInstance().GetPreset(rawPresetIdx);
		if (!preset) return;

		array<string> clothingList = ResolveClothing(pb, preset);
		for (int clothIdx = 0; clothIdx < clothingList.Count(); clothIdx++)
		{
			pb.GetInventory().CreateInInventory(clothingList[clothIdx]);
		}
		for (int gearIdx = 0; gearIdx < preset.Gear.Count(); gearIdx++)
		{
			DmGearItemData gearItem = preset.Gear[gearIdx];
			for (int copyIdx = 0; copyIdx < gearItem.Count; copyIdx++)
			{
				pb.GetInventory().CreateInInventory(gearItem.ClassName);
			}
		}
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(StageWeapons, 50, false, pb, rawPresetIdx);
	}

	// Hotbar layout: slot 1 gun, slots 2-3 bandages, slot 4 melee, slot 5
	// sidearm (when a preset carries both a primary and a secondary).
	void StageWeapons(PlayerBase pb, int rawPresetIdx)
	{
		if (!pb || !pb.IsAlive()) return;
		DmPresetData preset = DmPresetsConfig.GetInstance().GetPreset(rawPresetIdx);
		if (!preset) return;

		// The main weapon goes to hands AND hotbar slot 1. When a preset has
		// no primary (e.g. pistols-only), the secondary takes that role.
		bool secondaryIsMain = (preset.PrimaryClass == "" && preset.SecondaryClass != "");

		if (preset.PrimaryClass != "")
		{
			EntityAI primary = SpawnWeaponInHands(pb, preset.PrimaryClass);
			if (primary)
			{
				for (int attIdx = 0; attIdx < preset.PrimaryAttachments.Count(); attIdx++)
				{
					primary.GetInventory().CreateAttachment(preset.PrimaryAttachments[attIdx]);
				}
				LoadWeapon(primary, preset.PrimaryMagClass);
				if (preset.PrimaryMagClass != "")
				{
					// Spares beyond the loaded one go to the player inventory.
					for (int magIdx = 1; magIdx < preset.PrimaryMagCount; magIdx++)
					{
						pb.GetInventory().CreateInInventory(preset.PrimaryMagClass);
					}
				}
				pb.SetQuickBarEntityShortcut(primary, 0, true);
			}
		}

		if (preset.SecondaryClass != "")
		{
			EntityAI secondary;
			if (secondaryIsMain)
			{
				secondary = SpawnWeaponInHands(pb, preset.SecondaryClass);
			}
			else
			{
				secondary = EntityAI.Cast(pb.GetInventory().CreateInInventory(preset.SecondaryClass));
			}
			if (secondary)
			{
				for (int satIdx = 0; satIdx < preset.SecondaryAttachments.Count(); satIdx++)
				{
					secondary.GetInventory().CreateAttachment(preset.SecondaryAttachments[satIdx]);
				}
				LoadWeapon(secondary, preset.SecondaryMagClass);
				if (preset.SecondaryMagClass != "")
				{
					for (int smagIdx = 1; smagIdx < preset.SecondaryMagCount; smagIdx++)
					{
						pb.GetInventory().CreateInInventory(preset.SecondaryMagClass);
					}
				}
				if (secondaryIsMain)
				{
					pb.SetQuickBarEntityShortcut(secondary, 0, true);
				}
				else
				{
					pb.SetQuickBarEntityShortcut(secondary, 4, true);
				}
			}
		}

		BindBandages(pb);
		SpawnMelee(pb);
	}

	// First two bandage-type items from the gear land on hotbar slots 2-3.
	private void BindBandages(PlayerBase pb)
	{
		array<EntityAI> items = new array<EntityAI>;
		pb.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items);
		int slot = 1;
		for (int itemIdx = 0; itemIdx < items.Count(); itemIdx++)
		{
			EntityAI item = items[itemIdx];
			if (!item) continue;
			string typeName = item.GetType();
			if (typeName != "BandageDressing" && typeName != "Rag") continue;
			pb.SetQuickBarEntityShortcut(item, slot, true);
			slot++;
			if (slot > 2) break;
		}
	}

	// One random melee weapon from the validated pool, hotbar slot 4.
	private void SpawnMelee(PlayerBase pb)
	{
		if (!DmConfig.GetInstance().IsMeleeSpawnEnabled()) return;
		if (m_ValidMeleeClasses.Count() == 0) return;
		string meleeClass = m_ValidMeleeClasses[Math.RandomInt(0, m_ValidMeleeClasses.Count())];
		EntityAI melee = EntityAI.Cast(pb.GetInventory().CreateInInventory(meleeClass));
		if (!melee)
		{
			Print("[DM] melee spawn failed for '" + meleeClass + "' (inventory full?)");
			return;
		}
		pb.SetQuickBarEntityShortcut(melee, 3, true);
	}

	// Weapons spawn ready to fire: full magazine attached (or internal mag
	// filled) AND a round chambered, via the vanilla SpawnAmmo cascade, which
	// also fixes the weapon FSM and synchronizes to clients. magOrAmmoClass
	// follows SpawnAmmo semantics: a magazine classname for mag-fed weapons,
	// an ammo classname for internal-magazine weapons (Mosin, shotguns), or
	// "" to let the engine pick a chamberable type.
	private void LoadWeapon(EntityAI weaponEntity, string magOrAmmoClass)
	{
		Weapon_Base weapon = Weapon_Base.Cast(weaponEntity);
		if (!weapon) return;
		weapon.SpawnAmmo(magOrAmmoClass, WeaponWithAmmoFlags.CHAMBER);
	}

	// Hands first; if the engine refuses (hands blocked mid-transition),
	// fall back to inventory so the weapon is never silently lost.
	private EntityAI SpawnWeaponInHands(PlayerBase pb, string weaponClass)
	{
		EntityAI weapon = pb.GetHumanInventory().CreateInHands(weaponClass);
		if (!weapon)
		{
			weapon = EntityAI.Cast(pb.GetInventory().CreateInInventory(weaponClass));
		}
		return weapon;
	}

	static void SelfTest()
	{
		// Structural fixture only: classname resolution needs the live config
		// tree, which boot exercises via ValidateAll's log line. Here we prove
		// the valid-index indirection can't go out of bounds.
		DmLoadoutFactory probe = new DmLoadoutFactory();
		int boundsOk = 1;
		if (probe.GetValidPresetCount() != 0) boundsOk = 0;
		if (probe.GetValidPreset(0)) boundsOk = 0;
		if (probe.GetValidPreset(-1)) boundsOk = 0;
		if (probe.GetValidMeleeCount() != 0) boundsOk = 0;
		if (probe.GetValidMeleeClass(0) != "") boundsOk = 0;
		if (probe.GetValidMeleeClass(-1) != "") boundsOk = 0;
		Print("[DM] fixture DmLoadoutFactory bounds: expected=1 got=" + boundsOk.ToString() + " " + DmFixture.Verdict(boundsOk == 1));

		// Meta-preset member resolution: order follows RandomFrom, unknown
		// names are skipped, and a meta can never contain a meta.
		DmPresetsConfig presetProbe = DmPresetsConfig.MakeDefaultsProbe(); // SMG Rush(0), Shotgun CQB(1)
		array<int> validRaw = new array<int>;
		validRaw.Insert(0);
		validRaw.Insert(1);
		DmPresetData metaProbe = new DmPresetData();
		metaProbe.Name = "Free For All";
		metaProbe.RandomFrom.Insert("Shotgun CQB");
		metaProbe.RandomFrom.Insert("SMG Rush");
		metaProbe.RandomFrom.Insert("No Such Preset");
		array<int> members = ResolveMetaMembers(presetProbe, validRaw, metaProbe);
		int metaOk = 1;
		if (members.Count() != 2) metaOk = 0;
		if (members.Count() == 2 && (members[0] != 1 || members[1] != 0)) metaOk = 0;
		DmPresetData emptyMeta = new DmPresetData();
		emptyMeta.Name = "Empty";
		emptyMeta.RandomFrom.Insert("No Such Preset");
		if (ResolveMetaMembers(presetProbe, validRaw, emptyMeta).Count() != 0) metaOk = 0;
		Print("[DM] fixture DmLoadoutFactory meta members: expected=1 got=" + metaOk.ToString() + " " + DmFixture.Verdict(metaOk == 1));
	}
}
