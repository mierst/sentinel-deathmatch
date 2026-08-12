// Round-end scoreboard. Auto-opened at ROUNDEND by the HUD controller; also
// toggled with the scoreboard key during LIVE (shows the last-received rows).
class DmScoreboardMenu extends UIScriptedMenu
{
	private MultilineTextWidget m_RowsText;
	private TextWidget m_WinnerText;
	private ButtonWidget m_BtnClose;
	private ButtonWidget m_BtnRound;
	private ButtonWidget m_BtnSession;

	// false = this round, true = session leaderboard (totals across rounds).
	private bool m_ShowSession = false;

	override Widget Init()
	{
		layoutRoot = GetGame().GetWorkspace().CreateWidgets("SentinelDM/layouts/dm_scoreboard.layout");
		m_RowsText = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("RowsText"));
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

		if (m_RowsText)
		{
			string sourceBlob = state.m_ScoreboardBlob;
			if (m_ShowSession) sourceBlob = state.m_SessionBlob;

			// Rows arrive as name\tK\tD\tstreak lines; render with padded
			// columns (monospace-ish enough at this font size for v1).
			array<string> rows = new array<string>;
			DmClientState.SplitBlob(sourceBlob, rows);
			string rendered = "";
			for (int rowIdx = 0; rowIdx < rows.Count(); rowIdx++)
			{
				array<string> fields = new array<string>;
				rows[rowIdx].Split("\t", fields);
				if (fields.Count() < 4) continue;
				string paddedName = fields[0];
				while (paddedName.Length() < 40)
				{
					paddedName = paddedName + " ";
				}
				if (rowIdx > 0) rendered = rendered + "\n";
				rendered = rendered + paddedName + fields[1] + "      " + fields[2] + "      " + fields[3];
			}
			m_RowsText.SetText(rendered);
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
