// Isolated dedicated-server mission; never load on a production server.
// tools/ci/test-medical.ps1 supplies independently expected config values.
class DmMedicalSmokeExpectations
{
	string Mode;
	float IntervalSeconds;
	float HealthPerTick;
}

bool MedicalHealthNear(PlayerBase patient, float expected)
{
	return Math.AbsFloat(patient.GetHealth("GlobalHealth", "Health") - expected) < 0.001;
}
void MedicalCheck(string name, bool passed)
{
	Print("[DM-MEDICAL] " + name + " " + DmFixture.Verdict(passed));
}

void BenchmarkMedical()
{
	array<Man> crowd = new array<Man>;
	for (int spawnIdx = 0; spawnIdx < 60; spawnIdx++)
	{
		vector spawnPos = Vector(7500 + spawnIdx * 2, GetGame().SurfaceY(7500 + spawnIdx * 2, 7500), 7500);
		PlayerBase actor = PlayerBase.Cast(GetGame().CreatePlayer(null, "SurvivorM_Mirek", spawnPos, 0, "NONE"));
		if (actor) crowd.Insert(actor);
	}
	MedicalCheck("benchmark 60 bodies", crowd.Count() == 60);
	DmRoundEngine benchmark = new DmRoundEngine();
	benchmark.TickMedical(crowd, 1000);
	float elapsed = 0;
	float slowest = 0;
	float interval = DmConfig.GetInstance().GetRegenIntervalSeconds();
	for (int sampleIdx = 1; sampleIdx <= 100; sampleIdx++)
	{
		for (int prepIdx = 0; prepIdx < crowd.Count(); prepIdx++)
		{
			PlayerBase prepared = PlayerBase.Cast(crowd[prepIdx]);
			prepared.SetHealth("GlobalHealth", "Health", 50);
			prepared.GetBleedingManagerServer().AttemptAddBleedingSourceBySelection("LeftForeArmRoll");
		}
		float started = GetGame().GetTickTime();
		benchmark.TickMedical(crowd, 1000 + sampleIdx * interval);
		float sampleSeconds = GetGame().GetTickTime() - started;
		elapsed = elapsed + sampleSeconds;
		if (sampleSeconds > slowest) slowest = sampleSeconds;
	}
	Print("[DM-MEDICAL] benchmark 60 wounded/bleeding bodies, 100 due calls, mean_us=" + (elapsed * 10000).ToString());
	Print("[DM-MEDICAL] benchmark max_due_us=" + (slowest * 1000000).ToString());
	for (int deleteIdx = 0; deleteIdx < crowd.Count(); deleteIdx++) GetGame().ObjectDelete(crowd[deleteIdx]);
}

void RunMedicalSmoke()
{
	DmMedicalSmokeExpectations expected = new DmMedicalSmokeExpectations();
	if (!FileExist("$profile:medical-expectations.json"))
	{
		MedicalCheck("expectations file exists", false);
		return;
	}
	JsonFileLoader<DmMedicalSmokeExpectations>.JsonLoadFile("$profile:medical-expectations.json", expected);
	DmConfig config = DmConfig.GetInstance();
	bool expectedRegen = expected.Mode == "regen";
	MedicalCheck("configured mode, interval and HP", config.IsMedicalRegenEnabled() == expectedRegen && config.GetRegenIntervalSeconds() == expected.IntervalSeconds && config.GetRegenHealthPerTick() == expected.HealthPerTick);
	PlayerBase patient = PlayerBase.Cast(GetGame().CreatePlayer(null, "SurvivorM_Mirek", Vector(7500, GetGame().SurfaceY(7500, 7500), 7500), 0, "NONE"));
	if (!patient)
	{
		MedicalCheck("create patient", false);
		return;
	}
	array<Man> patients = new array<Man>;
	patients.Insert(null);
	patients.Insert(patient);
	patient.SetHealth("GlobalHealth", "Health", 50);
	patient.SetHealth("GlobalHealth", "Blood", 4000);
	patient.SetHealth("GlobalHealth", "Shock", 80);
	patient.GetBleedingManagerServer().AttemptAddBleedingSourceBySelection("LeftForeArmRoll");
	MedicalCheck("bleeding precondition", patient.GetBleedingSourceCount() > 0);
	DmRoundEngine engine = new DmRoundEngine();
	engine.TickMedical(patients, 100);
	engine.TickMedical(patients, 100 + expected.IntervalSeconds - 0.25);
	MedicalCheck("no early heal", patient.GetHealth("GlobalHealth", "Health") == 50 && patient.GetBleedingSourceCount() > 0);
	float firstDeadline = 100 + expected.IntervalSeconds;
	engine.TickMedical(patients, firstDeadline);
	if (!expectedRegen)
	{
		MedicalCheck("bandages preserves health and bleeding", patient.GetHealth("GlobalHealth", "Health") == 50 && patient.GetBleedingSourceCount() > 0);
	}
	else
	{
		float firstHealth = Math.Min(100, 50 + expected.HealthPerTick);
		MedicalCheck("configured HP and bleed clearing", MedicalHealthNear(patient, firstHealth) && patient.GetBleedingSourceCount() == 0);
		MedicalCheck("blood and shock unchanged", patient.GetHealth("GlobalHealth", "Blood") == 4000 && patient.GetHealth("GlobalHealth", "Shock") == 80);
		patient.SetHealth("GlobalHealth", "Health", 10);
		float afterStall = firstDeadline + expected.IntervalSeconds * 10;
		engine.TickMedical(patients, afterStall);
		float stallHealth = Math.Min(100, 10 + expected.HealthPerTick);
		MedicalCheck("stall heals once", MedicalHealthNear(patient, stallHealth));
		engine.TickMedical(patients, afterStall);
		engine.TickMedical(patients, afterStall + expected.IntervalSeconds - 0.25);
		MedicalCheck("no catch-up burst", MedicalHealthNear(patient, stallHealth));
		patient.SetHealth("GlobalHealth", "Health", 100 - expected.HealthPerTick * 0.5);
		engine.TickMedical(patients, afterStall + expected.IntervalSeconds);
		MedicalCheck("maximum HP cap", MedicalHealthNear(patient, 100));
		patient.GetBleedingManagerServer().AttemptAddBleedingSourceBySelection("LeftForeArmRoll");
		MedicalCheck("full HP bleeding precondition", patient.GetBleedingSourceCount() > 0);
		engine.TickMedical(patients, afterStall + expected.IntervalSeconds * 2);
		MedicalCheck("full HP still clears bleeds", patient.GetBleedingSourceCount() == 0);
		patient.SetHealth("GlobalHealth", "Health", 0);
		engine.TickMedical(patients, afterStall + expected.IntervalSeconds * 3);
		MedicalCheck("dead player remains dead", !patient.IsAlive() && patient.GetHealth("GlobalHealth", "Health") == 0);
	}
	GetGame().ObjectDelete(patient);
	BenchmarkMedical();
	Print("[DM-MEDICAL] smoke complete");
}

void main()
{
	Hive ce = CreateHive();
	if (ce) ce.InitOffline();
	GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RunMedicalSmoke, 1000, false);
}
