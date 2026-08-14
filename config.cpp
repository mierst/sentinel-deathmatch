// SentinelDM core mod manifest.
//
// requiredAddons is deliberately EMPTY. Listing vanilla addons (DZ_Data,
// DZ_Scripts) makes the Linux server silently reject the mod entry and never
// load the PBO (lesson inherited from sentinel-enforcer). Enforce Script base
// classes resolve via the script-module namespace, not CfgPatches.
class CfgPatches
{
	class SentinelDM
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {};
	};
};

class CfgMods
{
	class SentinelDM
	{
		dir = "SentinelDM";
		name = "Sentinel Deathmatch";
		author = "Sentinel";
		version = "0.1.12";
		type = "mod";
		dependencies[] = { "Game", "World", "Mission" };
		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = { "SentinelDM/scripts/3_Game" };
			};
			class worldScriptModule
			{
				value = "";
				files[] = { "SentinelDM/scripts/4_World" };
			};
			class missionScriptModule
			{
				value = "";
				files[] = { "SentinelDM/scripts/5_Mission" };
			};
		};
	};
};
