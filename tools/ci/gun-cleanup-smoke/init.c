void main() {}

class GunProbeMission: MissionServer
{
    ref DmCleanupService m_Probe;
    ref DmZoneData m_Zone;
    PlayerBase m_Body;
    PlayerBase m_Looter;
    PlayerBase m_RaceLooter;
    Weapon_Base m_DeathGun;
    Weapon_Base m_PickupGun;
    Weapon_Base m_ManualGun;
    Weapon_Base m_RaceGun;
    EntityAI m_GunBag;
    Weapon_Base m_ContainedGun;
    ref array<Weapon_Base> m_TransitionGuns = new array<Weapon_Base>;
    string m_Mode;

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
        m_Probe = new DmCleanupService();
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
        Check("setup", m_Body && m_Looter && m_RaceLooter && m_PickupGun && m_DeathGun && m_ManualGun && m_RaceGun);
        m_Body.SetHealth("GlobalHealth", "Health", 0);
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
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(BeforeRoundEnd, 2000, false);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(AfterRoundEnd, 4000, false);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(AtDeathDeadline, 11000, false);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(FinishGunProbe, 12000, false);
    }

    void BeforeRoundEnd()
    {
        Check("countdown preserves death gun", m_DeathGun != null);
        Check("countdown preserves manual gun", m_ManualGun != null);
        Check("countdown preserves gun container", m_GunBag && m_ContainedGun && m_ContainedGun.GetHierarchyParent() == m_GunBag);
        m_Probe.SweepGroundItems(m_Zone, true);
        m_Probe.ExpireAll();
        m_RaceLooter.LocalTakeEntityToHands(m_RaceGun);
        m_Probe.OnSweep();
    }

    void AfterRoundEnd()
    {
        Check("corpse cleared", m_Body == null);
        if (m_Mode == "round_end")
        {
            Check("round end removes death gun", m_DeathGun == null);
            Check("round end removes manual gun", m_ManualGun == null);
        }
        else
        {
            Check("round end preserves death deadline or server lifetime", m_DeathGun != null);
            Check("round end preserves manual gun", m_ManualGun != null);
        }
        Check("looted gun survives round end", m_PickupGun && m_PickupGun.GetHierarchyRootPlayer() == m_Looter);
        Check("pickup after queue survives", m_RaceGun && m_RaceGun.GetHierarchyRootPlayer() == m_RaceLooter);
    }

    void AtDeathDeadline()
    {
        m_Probe.OnSweep();
    }

    void FinishGunProbe()
    {
        if (m_Mode == "player_death") Check("ten second death cleanup", m_DeathGun == null);
        if (m_Mode == "server") Check("server lifetime preserved", m_DeathGun != null);
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
