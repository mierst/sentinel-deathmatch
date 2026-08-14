// Round-end scoreboard. Auto-opened at ROUNDEND by the HUD controller; also
// toggled with the scoreboard key during LIVE (shows the last-received rows).
class DmScoreboardMenu extends UIScriptedMenu
{
	// Four aligned columns (names left, numbers right): one multiline widget
	// per column beats per-row widgets and beats space-padding, which never
	// lines up in a proportional font.
	private MultilineTextWidget m_ColName;
	private MultilineTextWidget m_ColK;
	private MultilineTextWidget m_ColD;
	private MultilineTextWidget m_ColStreak;
	private TextWidget m_WinnerText;
	private ButtonWidget m_BtnClose;
	private ButtonWidget m_BtnRound;
	private ButtonWidget m_BtnSession;

	// false = this round, true = session leaderboard (totals across rounds).
	private bool m_ShowSession = false;

	override Widget Init()
	{
		layoutRoot = GetGame().GetWorkspace().CreateWidgets("SentinelDM/layouts/dm_scoreboard.layout");
		m_ColName = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("ColName"));
		m_ColK = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("ColK"));
		m_ColD = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("ColD"));
		m_ColStreak = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("ColStreak"));
		m_WinnerText = TextWidget.Cast(layoutRoot.FindAnyWidget("WinnerText"));
		m_BtnClose = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnClose"));
		m_BtnRound = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnRound"));
		m_BtnSession = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnSession"));
		Refresh();
		return layoutRoot;
	}

	override bool UseMouse() { return true; }
	override bool UseKeyboard() { return true; }

	// Same focus discipline as the vote menu: cursor drives the UI only.
	override void OnShow()
	{
		super.OnShow();
		SetFocus(layoutRoot);
		GetGame().GetInput().ChangeGameFocus(1);
		GetGame().GetUIManager().ShowUICursor(true);
	}

	override void OnHide()
	{
		super.OnHide();
		GetGame().GetInput().ResetGameFocus();
		GetGame().GetUIManager().ShowUICursor(false);
	}

	void Refresh()
	{
		DmClientState state = DmClientState.GetInstance();

		if (m_WinnerText)
		{
			if (m_ShowSession)
			{
				m_WinnerText.SetText("Session leaderboard");
			}
			else if (state.m_WinnerName != "")
			{
				m_WinnerText.SetText("Winner: " + state.m_WinnerName);
			}
			else
			{
				m_WinnerText.SetText("");
			}
		}

		if (m_ColName && m_ColK && m_ColD && m_ColStreak)
		{
			string sourceBlob = state.m_ScoreboardBlob;
			if (m_ShowSession) sourceBlob = state.m_SessionBlob;

			// Rows arrive as name\tK\tD\tstreak lines; fan the fields out
			// into the four column widgets, one line per row in each.
			array<string> rows = new array<string>;
			DmClientState.SplitBlob(sourceBlob, rows);
			string nameLines = "";
			string kLines = "";
			string dLines = "";
			string streakLines = "";
			for (int rowIdx = 0; rowIdx < rows.Count(); rowIdx++)
			{
				array<string> fields = new array<string>;
				rows[rowIdx].Split("\t", fields);
				if (fields.Count() < 4) continue;
				if (nameLines != "")
				{
					nameLines = nameLines + "\n";
					kLines = kLines + "\n";
					dLines = dLines + "\n";
					streakLines = streakLines + "\n";
				}
				nameLines = nameLines + fields[0];
				kLines = kLines + fields[1];
				dLines = dLines + fields[2];
				streakLines = streakLines + fields[3];
			}
			m_ColName.SetText(nameLines);
			m_ColK.SetText(kLines);
			m_ColD.SetText(dLines);
			m_ColStreak.SetText(streakLines);
		}
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (w == m_BtnClose)
		{
			Close();
			return true;
		}
		if (w == m_BtnRound)
		{
			m_ShowSession = false;
			Refresh();
			return true;
		}
		if (w == m_BtnSession)
		{
			m_ShowSession = true;
			Refresh();
			return true;
		}
		return super.OnClick(w, x, y, button);
	}
}
