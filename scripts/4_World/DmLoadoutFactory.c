// Preset validation and staged loadout injection.
//
// Injection is deliberately staged across frames via the CallQueue: clearing
// and refilling an inventory in the same frame is unreliable. Stage 1 clears,
// stage 2 dresses, stage 3 arms.
class DmLoadoutFactory
{
	private static ref DmLoadoutFactory s_Instance;

	private ref array<int> m_ValidPresetIndices = new array<int>;

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
		DmPresetsConfig presets = DmPresetsConfig.GetInstance();

		for (int presetIdx = 0; presetIdx < presets.GetPresetCount(); presetIdx++)
		{
			DmPresetData preset = presets.GetPreset(presetIdx);
			if (!preset.Enabled) continue;

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
		Print("[DM] presets.json: " + m_ValidPresetIndices.Count().ToString() + " of " + presets.GetPresetCount().ToString() + " presets valid");
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

	DmPresetData GetValidPreset(int validIdx)
	{
		if (validIdx < 0 || validIdx >= m_ValidPresetIndices.Count()) return null;
		return DmPresetsConfig.GetInstance().GetPreset(m_ValidPresetIndices[validIdx]);
	}

	// Apply a validated preset (by valid-index). Stage 1 runs next frame.
	void Apply(PlayerBase pb, int validIdx)
	{
		DmPresetData preset = GetValidPreset(validIdx);
		if (!pb || !preset) return;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(StageClear, 25, false, pb, validIdx);
	}

	void StageClear(PlayerBase pb, int validIdx)
	{
		if (!pb || !pb.IsAlive()) return;
		pb.ClearInventory();
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(StageClothing, 50, false, pb, validIdx);
	}

	void StageClothing(PlayerBase pb, int validIdx)
	{
		if (!pb || !pb.IsAlive()) return;
		DmPresetData preset = GetValidPreset(validIdx);
		if (!preset) return;

		for (int clothIdx = 0; clothIdx < preset.Clothing.Count(); clothIdx++)
		{
			pb.GetInventory().CreateInInventory(preset.Clothing[clothIdx]);
		}
		for (int gearIdx = 0; gearIdx < preset.Gear.Count(); gearIdx++)
		{
			DmGearItemData gearItem = preset.Gear[gearIdx];
			for (int copyIdx = 0; copyIdx < gearItem.Count; copyIdx++)
			{
				pb.GetInventory().CreateInInventory(gearItem.ClassName);
			}
		}
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(StageWeapons, 50, false, pb, validIdx);
	}

	void StageWeapons(PlayerBase pb, int validIdx)
	{
		if (!pb || !pb.IsAlive()) return;
		DmPresetData preset = GetValidPreset(validIdx);
		if (!preset) return;

		if (preset.PrimaryClass != "")
		{
			EntityAI primary = pb.GetHumanInventory().CreateInHands(preset.PrimaryClass);
			if (primary)
			{
				for (int attIdx = 0; attIdx < preset.PrimaryAttachments.Count(); attIdx++)
				{
					primary.GetInventory().CreateAttachment(preset.PrimaryAttachments[attIdx]);
				}
				if (preset.PrimaryMagClass != "")
				{
					primary.GetInventory().CreateAttachment(preset.PrimaryMagClass);
					// Spares beyond the loaded one go to the player inventory.
					for (int magIdx = 1; magIdx < preset.PrimaryMagCount; magIdx++)
					{
						pb.GetInventory().CreateInInventory(preset.PrimaryMagClass);
					}
				}
			}
		}

		if (preset.SecondaryClass != "")
		{
			EntityAI secondary = EntityAI.Cast(pb.GetInventory().CreateInInventory(preset.SecondaryClass));
			if (secondary)
			{
				for (int satIdx = 0; satIdx < preset.SecondaryAttachments.Count(); satIdx++)
				{
					secondary.GetInventory().CreateAttachment(preset.SecondaryAttachments[satIdx]);
				}
				if (preset.SecondaryMagClass != "")
				{
					secondary.GetInventory().CreateAttachment(preset.SecondaryMagClass);
					for (int smagIdx = 1; smagIdx < preset.SecondaryMagCount; smagIdx++)
					{
						pb.GetInventory().CreateInInventory(preset.SecondaryMagClass);
					}
				}
			}
		}
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
		Print("[DM] fixture DmLoadoutFactory bounds: expected=1 got=" + boundsOk.ToString() + " " + DmFixture.Verdict(boundsOk == 1));
	}
}
