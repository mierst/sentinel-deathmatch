void main() {}

class GunProbeExpected
{
    string Mode;
}

class GunProbeMission: MissionServer
{
    ref DmCleanupService m_Probe;
    ref DmZoneData m_Zone;
    ref DmRoundEngine m_CombatRound;
    PlayerBase m_Body;
    PlayerBase m_Looter;
    PlayerBase m_RaceLooter;
    Weapon_Base m_DeathGun;
    Weapon_Base m_PickupGun;
    Weapon_Base m_ManualGun;
    Weapon_Base m_RaceGun;
    PlayerBase m_EarlyBody;
    Weapon_Base m_EarlyGun;
    EntityAI m_GunBag;
    Weapon_Base m_ContainedGun;
    ref array<Weapon_Base> m_TransitionGuns = new array<Weapon_Base>;
    string m_Mode;
    float m_DeathStartedAt;
    float m_ObservedDeletionAt = -1;

    override void OnInit()
    {
        super.OnInit();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(SetupGunProbe, 1000, false);
    }

    void Check(string label, bool ok)
    {
        Print("[GUN-PROBE] " + label + " " + DmFixture.Verdict(ok));
    }

    void SetupGunProbe()
    {
        m_Mode = DmConfig.GetInstance().GetGunCleanupMode();
        GunProbeExpected expected = new GunProbeExpected();
        JsonFileLoader<GunProbeExpected>.JsonLoadFile("$profile:gun-cleanup-expected.json", expected);
        Check("configured mode", m_Mode == expected.Mode);
        m_Probe = new DmCleanupService();
        m_Probe.Start();
        m_CombatRound = new DmRoundEngine();
        m_CombatRound.TransitionTo(DmPhase.LIVE);
        vector pos = Vector(7500, GetGame().SurfaceY(7500, 7500), 7500);
        m_Body = PlayerBase.Cast(GetGame().CreatePlayer(null, "SurvivorM_Mirek", pos, 0, "NONE"));
        m_Looter = PlayerBase.Cast(GetGame().CreatePlayer(null, "SurvivorM_Boris", pos + "2 0 0", 0, "NONE"));
        m_RaceLooter = PlayerBase.Cast(GetGame().CreatePlayer(null, "SurvivorM_Denis", pos + "4 0 0", 0, "NONE"));
        m_PickupGun = Weapon_Base.Cast(m_Body.GetHumanInventory().CreateInHands("M4A1"));
        m_DeathGun = Weapon_Base.Cast(m_Body.GetInventory().CreateAttachment("AKM"));
        m_ManualGun = Weapon_Base.Cast(GetGame().CreateObjectEx("Mosin9130", pos + "1 0 0", ECE_PLACE_ON_SURFACE));
        m_RaceGun = Weapon_Base.Cast(GetGame().CreateObjectEx("AK74", pos + "3 0 0", ECE_PLACE_ON_SURFACE));
        m_GunBag = EntityAI.Cast(GetGame().CreateObjectEx("MountainBag_Blue", pos + "5 0 0", ECE_PLACE_ON_SURFACE));
        m_ContainedGun = Weapon_Base.Cast(m_GunBag.GetInventory().CreateInInventory("MakarovIJ70"));
        bool setupOk = m_Body && m_Looter && m_RaceLooter && m_PickupGun && m_DeathGun && m_ManualGun && m_RaceGun && m_GunBag && m_ContainedGun;
        Check("setup", setupOk);
        if (!setupOk) return;
        m_Body.ProcessDirectDamage(DT_FIRE_ARM, m_Looter, "Torso", "Bullet_556x45", "0 0 0", 1000.0);
        Check("combat gunshot death", !m_Body.IsAlive() && m_Body.m_DmLastAttacker == m_Looter);
        // Synthetic players have no network identity. Register the corpse
        // directly; ordinary clients reach this through OnPlayerKilled.
        m_DeathStartedAt = GetGame().GetTickTime();
        m_Probe.RegisterCorpse(m_Body, m_PickupGun);
        Check("corpse gun released", !m_DeathGun.GetHierarchyParent());
        m_Looter.LocalTakeEntityToHands(m_PickupGun);
        Check("looted gun owned", m_PickupGun.GetHierarchyRootPlayer() == m_Looter);
        m_Zone = new DmZoneData();
        m_Zone.CenterX = 7500;
        m_Zone.CenterZ = 7500;
        m_Zone.Radius = 20;
        m_Zone.WarnMargin = 0;
        m_Probe.SweepGroundItems(m_Zone);
        m_Probe.OnSweep();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(BeforeDeathDeadline, 9000, false);
        if (m_Mode == "player_death") GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ObserveDeathCleanup, 9000, false);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DuringCombat, 12000, false);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(BeforeRoundEnd, 14000, false);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(AfterRoundEnd, 19000, false);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(FinishGunProbe, 20000, false);
    }

    void BeforeDeathDeadline()
    {
        Check("LIVE before deadline", m_CombatRound.GetPhase() == DmPhase.LIVE);
        Check("no cleanup before ten seconds", m_DeathGun != null);
        Check("manual gun survives combat", m_ManualGun != null);
        Check("looted gun survives combat", m_PickupGun && m_PickupGun.GetHierarchyRootPlayer() == m_Looter);
    }

    void ObserveDeathCleanup()
    {
        float elapsed = GetGame().GetTickTime() - m_DeathStartedAt;
        if (!m_DeathGun)
        {
            m_ObservedDeletionAt = elapsed;
            Print("[GUN-PROBE] observed death gun deletion seconds=" + elapsed.ToString());
            return;
        }
        if (elapsed < 12.0) GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ObserveDeathCleanup, 100, false);
    }

    void DuringCombat()
    {
        Check("LIVE after deadline", m_CombatRound.GetPhase() == DmPhase.LIVE);
        bool shouldRemain = m_Mode != "player_death";
        Check("death policy during combat", (m_DeathGun != null) == shouldRemain);
        if (m_Mode == "player_death") Check("death cleanup within sweep tolerance", m_ObservedDeletionAt >= 10.0 && m_ObservedDeletionAt <= 12.0);
    }

    void BeforeRoundEnd()
    {
        Check("manual gun survives until round end", m_ManualGun != null);
        Check("countdown preserves gun container", m_GunBag && m_ContainedGun && m_ContainedGun.GetHierarchyParent() == m_GunBag);
        vector lateDeathPos = Vector(7506, GetGame().SurfaceY(7506, 7500), 7500);
        m_EarlyBody = PlayerBase.Cast(GetGame().CreatePlayer(null, "SurvivorM_Peter", lateDeathPos, 0, "NONE"));
        // Use a shoulder gun here: an identity-less player's vanilla hand
        // drop queues a client inventory acknowledgement that this harness
        // cannot provide before immediate round-end corpse deletion.
        m_EarlyGun = Weapon_Base.Cast(m_EarlyBody.GetInventory().CreateAttachment("AKM"));
        Check("late death setup", m_EarlyBody && m_EarlyGun);
        if (!m_EarlyBody || !m_EarlyGun) return;
        m_EarlyBody.SetHealth("GlobalHealth", "Health", 0);
        m_Probe.RegisterCorpse(m_EarlyBody, m_EarlyGun);
        m_CombatRound.TransitionTo(DmPhase.ROUNDEND);
        m_Probe.SweepGroundItems(m_Zone, true);
        m_Probe.ExpireAll();
        m_RaceLooter.LocalTakeEntityToHands(m_RaceGun);
        m_Probe.OnSweep();
    }

    void AfterRoundEnd()
    {
        Check("corpse cleared", m_Body == null);
        Check("early round end keeps death deadline", (m_EarlyGun != null) == (m_Mode != "round_end"));
        if (m_Mode == "round_end")
        {
            Check("round end death gun policy", m_DeathGun == null);
            Check("round end manual gun policy", m_ManualGun == null);
        }
        else
        {
            Check("round end death gun policy", (m_DeathGun != null) == (m_Mode == "server"));
            Check("round end manual gun policy", m_ManualGun != null);
        }
        Check("looted gun survives round end", m_PickupGun && m_PickupGun.GetHierarchyRootPlayer() == m_Looter);
        Check("pickup after queue survives", m_RaceGun && m_RaceGun.GetHierarchyRootPlayer() == m_RaceLooter);
    }

    void FinishGunProbe()
    {
        Check("looted gun survives death deadline", m_PickupGun && m_PickupGun.GetHierarchyRootPlayer() == m_Looter);
        DmZoneService.GetInstance().SetActiveZone(m_Zone);
        array<int> destinations = {DmPhase.ROUNDEND, DmPhase.VOTING, DmPhase.IDLE};
        for (int phaseIdx = 0; phaseIdx < destinations.Count(); phaseIdx++)
        {
            vector testPos = Vector(7500 + phaseIdx, GetGame().SurfaceY(7500 + phaseIdx, 7500), 7500);
            Weapon_Base transitionGun = Weapon_Base.Cast(GetGame().CreateObjectEx("AK74", testPos, ECE_PLACE_ON_SURFACE));
            m_TransitionGuns.Insert(transitionGun);
            DmRoundEngine transitionEngine = new DmRoundEngine();
            transitionEngine.TransitionTo(DmPhase.LIVE);
            transitionEngine.TransitionTo(destinations[phaseIdx]);
        }
        DmCleanupService.GetInstance().OnSweep();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CheckTransitions, 2000, false);
    }

    void CheckTransitions()
    {
        for (int transitionIdx = 0; transitionIdx < m_TransitionGuns.Count(); transitionIdx++)
        {
            bool shouldRemain = m_Mode != "round_end";
            bool remains = m_TransitionGuns[transitionIdx] != null;
            Check("LIVE exit " + transitionIdx.ToString() + " follows mode", remains == shouldRemain);
        }
        Print("[GUN-PROBE] complete " + m_Mode);
    }
}

Mission CreateCustomMission(string path) { return new GunProbeMission(); }
