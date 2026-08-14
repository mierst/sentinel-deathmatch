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

	private int m_SeenVoteSeq = 0;
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
	}

	private void HandleSequences(DmClientState state)
	{
		// New vote window -> open the vote menu (unless another menu is up).
		if (state.m_VoteSeq != m_SeenVoteSeq)
		{
			m_SeenVoteSeq = state.m_VoteSeq;
			CloseScoreboard();
			OpenVoteMenu();
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
