// Isolated dedicated-server mission; never load on a production server.
// Run once with missing medical fields and once with regen/5 seconds/5 HP.
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
	for (int sampleIdx = 1; sampleIdx <= 100; sampleIdx++)
	{
		for (int prepIdx = 0; prepIdx < crowd.Count(); prepIdx++)
		{
			PlayerBase prepared = PlayerBase.Cast(crowd[prepIdx]);
			prepared.SetHealth("GlobalHealth", "Health", 50);
			prepared.GetBleedingManagerServer().AttemptAddBleedingSourceBySelection("LeftForeArmRoll");
		}
		float started = GetGame().GetTickTime();
		benchmark.TickMedical(crowd, 1000 + sampleIdx * 5);
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
	engine.TickMedical(patients, 104.5);
	MedicalCheck("no early heal", patient.GetHealth("GlobalHealth", "Health") == 50 && patient.GetBleedingSourceCount() > 0);
	engine.TickMedical(patients, 105);
	if (!DmConfig.GetInstance().IsMedicalRegenEnabled())
	{
		MedicalCheck("bandages preserves health and bleeding", patient.GetHealth("GlobalHealth", "Health") == 50 && patient.GetBleedingSourceCount() > 0);
	}
	else
	{
		MedicalCheck("configured HP and bleed clearing", patient.GetHealth("GlobalHealth", "Health") == 55 && patient.GetBleedingSourceCount() == 0);
		MedicalCheck("blood and shock unchanged", patient.GetHealth("GlobalHealth", "Blood") == 4000 && patient.GetHealth("GlobalHealth", "Shock") == 80);
		engine.TickMedical(patients, 150);
		MedicalCheck("stall heals once", patient.GetHealth("GlobalHealth", "Health") == 60);
		engine.TickMedical(patients, 150);
		engine.TickMedical(patients, 154.5);
		MedicalCheck("no catch-up burst", patient.GetHealth("GlobalHealth", "Health") == 60);
		patient.SetHealth("GlobalHealth", "Health", 98);
		engine.TickMedical(patients, 155);
		MedicalCheck("maximum HP cap", patient.GetHealth("GlobalHealth", "Health") == 100);
		patient.GetBleedingManagerServer().AttemptAddBleedingSourceBySelection("LeftForeArmRoll");
		MedicalCheck("full HP bleeding precondition", patient.GetBleedingSourceCount() > 0);
		engine.TickMedical(patients, 160);
		MedicalCheck("full HP still clears bleeds", patient.GetBleedingSourceCount() == 0);
		patient.SetHealth("GlobalHealth", "Health", 0);
		engine.TickMedical(patients, 165);
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
