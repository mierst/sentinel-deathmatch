// dm_sentinel: consumer-side bridge between Sentinel Deathmatch and the
// Sentinel Enforcer telemetry platform.
//
// This PBO ships as its own mod folder (@SentinelDeathmatch-Sentinel), loaded
// via -serverMod= ONLY on servers that run both mods. It must never be
// bundled into the core @SentinelDeathmatch folder: the core PBO downloads to
// clients, the enforcer is server-side only, and a client-loaded PBO with a
// hard requiredAddons on the enforcer fires a blocking "requires addon"
// dialog on every connect.
//
// requiredAddons does double duty: load order (both dependencies compile
// first) and a boot gate (this PBO refuses to load if either is missing).
class CfgPatches
{
	class SentinelDM_Sentinel
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = { "SentinelDM", "SentinelEnforcer" };
	};
};

class CfgMods
{
	class SentinelDM_Sentinel
	{
		dir = "SentinelDM_Sentinel";
		name = "Sentinel Deathmatch - Sentinel Bridge";
		author = "Sentinel";
		version = "0.1.0";
		type = "mod";
		dependencies[] = { "World", "Mission" };
		class defs
		{
			class worldScriptModule
			{
				value = "";
				files[] = { "SentinelDM_Sentinel/scripts/4_World" };
			};
			class missionScriptModule
			{
				value = "";
				files[] = { "SentinelDM_Sentinel/scripts/5_Mission" };
			};
		};
	};
};
