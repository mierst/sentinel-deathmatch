// Client-side guard for 3D-model scopes whose rear lens glass is a "hide"
// AnimationSource (several modded weapon-pack optics; vanilla has none that
// matter). Vanilla hides that selection CLIENT-side only on optics entry
// (ItemOptics.OnOpticEnter -> HideSelection("hide")) and shows it again on
// exit. Whenever that glass becomes visible while the player is still in the
// scope, the lens renders as a solid black disc with the scope body intact,
// cured only by leaving and re-entering ADS - the "scope goes black after
// firing" report (MWP Elcan Specter on a server-issued rifle, 09-24). The
// re-show was reproduced on the testbed by forcing the selection visible; the
// trigger in the field is not script-visible, so this guard re-asserts the
// hide instead of chasing it.
//
// Cost statement (CONTRIBUTING "no per-frame work"): this is the ONE accepted
// exception. It runs only inside DayZPlayerCameraOptics.OnUpdate, a camera
// that exists solely while the LOCAL player looks through a scope, on the
// client, for one entity: one native GetAnimationPhase per frame while the
// scope is a guarded one; optics without the animation (all vanilla ones) are
// detected within 0.25 s of first use and cost one int compare after that.
// Nothing runs on the dedicated server, where this camera is never created.
class DmOpticsGuard
{
	// Per-optic guard state. The config cannot tell us whether the MODEL has a
	// "hide" animation: ConfigIsExisting("... AnimationSources hide") answers
	// yes even for vanilla ACOGOptic (tested 09-24, inherited from a base
	// class), and its phase then reads 0 forever. So each optic is probed
	// once: vanilla has already
	// called HideSelection on entry, and a real animation reads phase 1 within
	// ~10 ms. If it never does, the guard switches itself off for that optic.
	static const int STATE_PROBE  = 0;
	static const int STATE_ACTIVE = 1;
	static const int STATE_OFF    = 2;
	static const float PROBE_SECONDS = 0.25;

	// Pure: next state from the phase read this frame and the probe age.
	static int NextState(int state, float hidePhase, float probeAge)
	{
		if (state == STATE_PROBE)
		{
			if (hidePhase >= 0.5) return STATE_ACTIVE;
			if (probeAge > PROBE_SECONDS) return STATE_OFF;
			return STATE_PROBE;
		}
		return state;
	}

	// Pure: re-hide only for a calibrated optic whose glass is visible or on
	// its way there (animPeriod moves the phase over ~10 ms).
	static bool NeedsRehide(int state, float hidePhase)
	{
		if (state != STATE_ACTIVE) return false;
		return hidePhase < 0.5;
	}

	static void SelfTest()
	{
		int guardOk = 1;
		// Probe: a phase of 1 (vanilla's own hide took) activates the guard.
		if (DmOpticsGuard.NextState(STATE_PROBE, 1.0, 0.0) != STATE_ACTIVE) guardOk = 0;
		// Probe: phase still 0 within the window keeps probing...
		if (DmOpticsGuard.NextState(STATE_PROBE, 0.0, 0.1) != STATE_PROBE) guardOk = 0;
		// ...and past the window the optic has no such animation: off for good.
		if (DmOpticsGuard.NextState(STATE_PROBE, 0.0, 0.3) != STATE_OFF) guardOk = 0;
		if (DmOpticsGuard.NextState(STATE_OFF, 1.0, 5.0) != STATE_OFF) guardOk = 0;
		if (DmOpticsGuard.NextState(STATE_ACTIVE, 0.0, 5.0) != STATE_ACTIVE) guardOk = 0;
		// Re-hide: only when active and the glass is shown or being shown.
		if (DmOpticsGuard.NeedsRehide(STATE_OFF, 0.0)) guardOk = 0;
		if (DmOpticsGuard.NeedsRehide(STATE_PROBE, 0.0)) guardOk = 0;
		if (DmOpticsGuard.NeedsRehide(STATE_ACTIVE, 1.0)) guardOk = 0;
		if (!DmOpticsGuard.NeedsRehide(STATE_ACTIVE, 0.0)) guardOk = 0;
		if (!DmOpticsGuard.NeedsRehide(STATE_ACTIVE, 0.3)) guardOk = 0;
		Print("[DM] fixture DmOpticsGuard state machine: expected=1 got=" + guardOk.ToString() + " " + DmFixture.Verdict(guardOk == 1));
	}
}

// THE single `modded class DayZPlayerCameraOptics` block for this mod.
modded class DayZPlayerCameraOptics
{
	ItemOptics m_DmGuardOptic;
	int m_DmGuardState;
	float m_DmGuardProbeAge;
	static int s_DmGuardRehides;
	static int s_DmGuardStateLogs;

	override void OnUpdate(float pDt, out DayZPlayerCameraResult pOutResult)
	{
		super.OnUpdate(pDt, pOutResult);

		ItemOptics guardOptic = ItemOptics.Cast(GetCurrentSightEntity());
		if (!guardOptic) return;
		if (guardOptic != m_DmGuardOptic)
		{
			m_DmGuardOptic = guardOptic;
			m_DmGuardState = DmOpticsGuard.STATE_PROBE;
			m_DmGuardProbeAge = 0.0;
		}
		if (m_DmGuardState == DmOpticsGuard.STATE_OFF) return;

		// Not while leaving the scope: OnOpticExit shows the glass on purpose.
		DayZPlayerImplement guardPlayer = DayZPlayerImplement.Cast(m_pPlayer);
		if (!guardPlayer || !guardPlayer.IsInOptics()) return;

		float guardPhase = guardOptic.GetAnimationPhase("hide");
		if (m_DmGuardState == DmOpticsGuard.STATE_PROBE)
		{
			m_DmGuardProbeAge = m_DmGuardProbeAge + pDt;
			m_DmGuardState = DmOpticsGuard.NextState(m_DmGuardState, guardPhase, m_DmGuardProbeAge);
			// A fresh camera (and probe) per ADS entry: log the first few only.
			if (m_DmGuardState != DmOpticsGuard.STATE_PROBE && s_DmGuardStateLogs < 5)
			{
				s_DmGuardStateLogs = s_DmGuardStateLogs + 1;
				Print("[DM] optics guard: " + guardOptic.GetType() + " state=" + m_DmGuardState.ToString() + " (1=guarded, 2=no hide animation)");
			}
			return;
		}

		if (!DmOpticsGuard.NeedsRehide(m_DmGuardState, guardPhase)) return;

		guardOptic.HideSelection("hide");
		s_DmGuardRehides = s_DmGuardRehides + 1;
		// The first few per session go to the client script log; enough to
		// confirm the guard fired without turning a spray into a log storm.
		if (s_DmGuardRehides <= 5)
		{
			Print("[DM] optics guard: re-hid lens glass of " + guardOptic.GetType() + " mid-ADS (#" + s_DmGuardRehides.ToString() + ")");
		}
	}
}
