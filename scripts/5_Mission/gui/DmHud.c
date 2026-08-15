// Client HUD controller. One 500 ms GUI timer drives everything: countdown
// text from the synced deadline, killfeed rows from the state ring, zone
// warning from own position vs the synced boundary, and menu auto-open when
// state sequence counters change. No per-frame client script.
class DmHudController
{
	private Widget m_Root;
	private TextWidget m_PhaseText;
	private TextWidget m_TimerText;
	private TextWidget m_InfoText;
	private TextWidget m_WarnText;
	private ref array<TextWidget> m_KillfeedRows = new array<TextWidget>;
	private ref array<Widget> m_KillfeedRowBgs = new array<Widget>;

	private ref Timer m_UpdateTimer;

	// Server-driven respawn leaves the vanilla death-screen focus state
	// behind (free cursor, UI focus) because the client never clicks the
	// respawn button that would normally dismiss it. Watch for our player
	// entity being swapped for a live one and restore game focus.
	private Man m_LastOwnPlayer;

	private int m_SeenVoteSeq = 0;
	// Vote-menu auto-open must survive a blocked first attempt: any open
	// menu (the CHAT INPUT included - both playtesters were mid-/mapvote)
	// vetoes ShowScriptedMenu, and a one-shot open left players voteless in
	// a running vote window (live 08-14). Retry every tick while wanted.
	private bool m_VoteMenuWanted = false;
	private int m_SeenScoreboardSeq = 0;
	private int m_SeenVoteResultSeq = 0;
	private float m_InfoClearAt = 0;

	private ref DmVoteMenu m_VoteMenu;
	private ref DmScoreboardMenu m_ScoreboardMenu;

	void Init()
	{
		m_Root = GetGame().GetWorkspace().CreateWidgets("SentinelDM/layouts/dm_hud.layout");
		if (!m_Root)
		{
			Print("[DM] WARNING: HUD layout failed to load");
			return;
		}

		m_PhaseText = TextWidget.Cast(m_Root.FindAnyWidget("PhaseText"));
		m_TimerText = TextWidget.Cast(m_Root.FindAnyWidget("TimerText"));
		m_InfoText = TextWidget.Cast(m_Root.FindAnyWidget("InfoText"));
		m_WarnText = TextWidget.Cast(m_Root.FindAnyWidget("WarnText"));
		for (int rowIdx = 0; rowIdx < 5; rowIdx++)
		{
			m_KillfeedRows.Insert(TextWidget.Cast(m_Root.FindAnyWidget("kf_" + rowIdx.ToString())));
			m_KillfeedRowBgs.Insert(m_Root.FindAnyWidget("kfbg_" + rowIdx.ToString()));
		}

		m_UpdateTimer = new Timer(CALL_CATEGORY_GUI);
		m_UpdateTimer.Run(0.5, this, "OnTick", null, true);
		// Pay the smoke-particle asset load now, not at first zone approach.
		GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(PrewarmMarkers, 5000, false);
		Print("[DM] HUD initialized");
	}

	void OnTick()
	{
		DmClientState state = DmClientState.GetInstance();
		float nowSeconds = GetGame().GetTickTime();

		UpdatePhaseAndTimer(state);
		UpdateKillfeed(state, nowSeconds);
		DmZoneMarkers.GetInstance().Update(state);
		UpdateZoneWarning(state);
		UpdateInfo(state, nowSeconds);
		HandleSequences(state);
		WatchRespawnFocus();
	}

	// Auto-respawn: the engine's respawn login can ONLY be opened by the
	// client (g_Game.RespawnPlayer() - the same native call the ESC-menu
	// Respawn button makes; no server-side equivalent exists, which is why
	// the old server-timer respawn raced the login FSM). This presses that
	// invisible button 2 s after death, so the player never touches a menu.
	private bool m_AutoRespawnRequested = false;

	private void WatchRespawnFocus()
	{
		Man ownPlayer = GetGame().GetPlayer();
		if (ownPlayer && ownPlayer.IsAlive() && m_LastOwnPlayer && ownPlayer != m_LastOwnPlayer)
		{
			// Fresh body: give the death-screen leftovers a beat to settle,
			// then reclaim focus (guarded - never fights a real menu).
			GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(RestoreGameFocus, 250, false);
		}
		if (ownPlayer)
		{
			if (ownPlayer.IsAlive())
			{
				m_AutoRespawnRequested = false;
			}
			else if (!m_AutoRespawnRequested)
			{
				m_AutoRespawnRequested = true;
				GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(RequestAutoRespawn, 2000, false);
			}
			m_LastOwnPlayer = ownPlayer;
		}
	}

	// Mirrors the vanilla InGameMenu respawn button's sequence exactly.
	void RequestAutoRespawn()
	{
		Man ownPlayer = GetGame().GetPlayer();
		if (!ownPlayer || ownPlayer.IsAlive()) return;
		if (!g_Game.CanRespawnPlayer())
		{
			// Engine not ready yet (death still settling): retry shortly.
			GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(RequestAutoRespawn, 1000, false);
			return;
		}
		g_Game.GetMenuDefaultCharacterData(false).SetRandomCharacterForced(true);
		g_Game.RespawnPlayer();
		PlayerBase deadBody = PlayerBase.Cast(ownPlayer);
		if (deadBody)
		{
			deadBody.SimulateDeath(true);
			GetGame().GetCallQueue(CALL_CATEGORY_GUI).Call(deadBody.ShowDeadScreen, true, 0);
		}
	}

	void RestoreGameFocus()
	{
		if (GetGame().GetUIManager().GetMenu()) return; // a menu owns the cursor
		Man ownPlayer = GetGame().GetPlayer();
		if (!ownPlayer || !ownPlayer.IsAlive()) return;
		GetGame().GetInput().ResetGameFocus();
		GetGame().GetUIManager().ShowUICursor(false);
		Mission mission = GetGame().GetMission();
		if (mission) mission.PlayerControlEnable(true);
	}

	void PrewarmMarkers()
	{
		DmZoneMarkers.GetInstance().Prewarm();
	}

	private void UpdatePhaseAndTimer(DmClientState state)
	{
		if (m_PhaseText)
		{
			m_PhaseText.SetText(DmPhase.Name(state.m_Phase));
		}
		if (m_TimerText)
		{
			int remainTotal = state.GetPhaseRemaining();
			int remainMin = remainTotal / 60;
			int remainSec = remainTotal % 60;
			string secPad = remainSec.ToString();
			if (remainSec < 10) secPad = "0" + secPad;
			m_TimerText.SetText(remainMin.ToString() + ":" + secPad);
		}
	}

	private void UpdateKillfeed(DmClientState state, float nowSeconds)
	{
		for (int rowIdx = 0; rowIdx < m_KillfeedRows.Count(); rowIdx++)
		{
			TextWidget row = m_KillfeedRows[rowIdx];
			if (!row) continue;

			bool rowLive = false;
			if (rowIdx < state.m_KillfeedLines.Count())
			{
				// Lines expire off the HUD after 8 s.
				if (nowSeconds - state.m_KillfeedTimes[rowIdx] < 8.0)
				{
					row.SetText(state.m_KillfeedLines[rowIdx]);
					rowLive = true;
				}
			}
			row.Show(rowLive);
			if (rowIdx < m_KillfeedRowBgs.Count() && m_KillfeedRowBgs[rowIdx])
			{
				m_KillfeedRowBgs[rowIdx].Show(rowLive);
			}
		}
	}

	private void UpdateZoneWarning(DmClientState state)
	{
		if (!m_WarnText) return;

		bool warn = false;
		if (state.m_Phase == DmPhase.LIVE)
		{
			Man ownPlayer = GetGame().GetPlayer();
			if (ownPlayer)
			{
				float overshoot = state.OwnOvershoot(ownPlayer.GetPosition());
				// Warn inside the margin band and anywhere beyond.
				if (overshoot > -state.m_ZoneWarnMargin) warn = true;
			}
		}
		if (warn)
		{
			// Append the server's authoritative grace countdown while it is
			// fresh (the server sends roughly one update per second).
			float countdownAge = GetGame().GetTickTime() - state.m_ZoneCountdownAt;
			if (state.m_ZoneCountdownAt > 0 && countdownAge < 2.0)
			{
				m_WarnText.SetText("RETURN TO THE ZONE - " + state.m_ZoneCountdownSeconds.ToString());
			}
			else
			{
				m_WarnText.SetText("RETURN TO THE ZONE");
			}
		}
		m_WarnText.Show(warn);
	}

	private void UpdateInfo(DmClientState state, float nowSeconds)
	{
		if (!m_InfoText) return;
		if (m_InfoClearAt > 0 && nowSeconds >= m_InfoClearAt)
		{
			m_InfoText.Show(false);
			m_InfoClearAt = 0;
		}
		// Standing hint whenever a vote is running and the menu isn't up
		// (blocked auto-open, or the player closed it): B reopens it.
		if (m_InfoClearAt == 0)
		{
			bool voteMenuOpen = m_VoteMenu && GetGame().GetUIManager().GetMenu() == m_VoteMenu;
			if (state.m_Phase == DmPhase.VOTING && !voteMenuOpen)
			{
				m_InfoText.SetText("Voting open - press B to choose arena + weapons");
				m_InfoText.Show(true);
			}
			else
			{
				m_InfoText.Show(false);
			}
		}
	}

	private void HandleSequences(DmClientState state)
	{
		// New vote window -> the vote menu is wanted until it actually opens
		// (or the window ends). OpenVoteMenu itself never fights another menu.
		if (state.m_VoteSeq != m_SeenVoteSeq)
		{
			m_SeenVoteSeq = state.m_VoteSeq;
			CloseScoreboard();
			m_VoteMenuWanted = true;
		}
		if (m_VoteMenuWanted)
		{
			if (state.m_Phase != DmPhase.VOTING)
			{
				m_VoteMenuWanted = false;
			}
			else if (m_VoteMenu && GetGame().GetUIManager().GetMenu() == m_VoteMenu)
			{
				m_VoteMenuWanted = false; // opened; a manual close now sticks
			}
			else
			{
				OpenVoteMenu();
			}
		}

		// New scoreboard data: auto-open ONLY at round end. During LIVE the
		// server pushes standings after every kill (the in-round leaderboard);
		// an open scoreboard refreshes from state each tick, so mid-round
		// updates land silently.
		if (state.m_ScoreboardSeq != m_SeenScoreboardSeq)
		{
			m_SeenScoreboardSeq = state.m_ScoreboardSeq;
			if (state.m_Phase == DmPhase.ROUNDEND)
			{
				CloseVoteMenu();
				OpenScoreboard();
			}
		}

		// Vote result -> transient info line.
		if (state.m_VoteResultSeq != m_SeenVoteResultSeq)
		{
			m_SeenVoteResultSeq = state.m_VoteResultSeq;
			CloseVoteMenu();
			if (m_InfoText)
			{
				m_InfoText.SetText(state.m_VoteResultText);
				m_InfoText.Show(true);
				m_InfoClearAt = GetGame().GetTickTime() + 8.0;
			}
		}

		// Keep the open vote menu's countdown fresh.
		if (m_VoteMenu && GetGame().GetUIManager().GetMenu() == m_VoteMenu)
		{
			m_VoteMenu.UpdateTimer();
		}
	}

	void OpenVoteMenu()
	{
		UIManager ui = GetGame().GetUIManager();
		if (ui.GetMenu()) return; // never fight another open menu (inventory etc.)
		if (!m_VoteMenu) m_VoteMenu = new DmVoteMenu();
		ui.ShowScriptedMenu(m_VoteMenu, null);
		m_VoteMenu.RefreshOptions();
	}

	void CloseVoteMenu()
	{
		if (m_VoteMenu && GetGame().GetUIManager().GetMenu() == m_VoteMenu)
		{
			m_VoteMenu.Close();
		}
	}

	void OpenScoreboard()
	{
		UIManager ui = GetGame().GetUIManager();
		if (ui.GetMenu()) return;
		if (!m_ScoreboardMenu) m_ScoreboardMenu = new DmScoreboardMenu();
		ui.ShowScriptedMenu(m_ScoreboardMenu, null);
		m_ScoreboardMenu.Refresh();
	}

	void CloseScoreboard()
	{
		if (m_ScoreboardMenu && GetGame().GetUIManager().GetMenu() == m_ScoreboardMenu)
		{
			m_ScoreboardMenu.Close();
		}
	}

	// Keybind entries (from MissionGameplay.OnKeyPress).
	void ToggleVoteMenu()
	{
		if (m_VoteMenu && GetGame().GetUIManager().GetMenu() == m_VoteMenu)
		{
			m_VoteMenu.Close();
			return;
		}
		if (DmClientState.GetInstance().m_Phase == DmPhase.VOTING)
		{
			OpenVoteMenu();
		}
	}

	void ToggleScoreboard()
	{
		if (m_ScoreboardMenu && GetGame().GetUIManager().GetMenu() == m_ScoreboardMenu)
		{
			m_ScoreboardMenu.Close();
			return;
		}
		OpenScoreboard();
	}
}
