// Vote screen. Opened by the HUD controller when a vote window arrives; one
// click per column casts (re-click to change); every cast sends the current
// selection pair - the server rate-limits and bounds-checks.
//
// A column with no options is rolled by the server (ArenaSelection /
// PresetSelection "random"): its header says so and it has no buttons. When
// the server allows the Random pick (CLIENT_OPTS), each voted column with at
// least two options grows a RANDOM button below the real ones; its vote
// index is the option count, which the server treats as "roll this column".
class DmVoteMenu extends UIScriptedMenu
{
	private ref array<ButtonWidget> m_ZoneButtons = new array<ButtonWidget>;
	private ref array<ButtonWidget> m_PresetButtons = new array<ButtonWidget>;
	private ButtonWidget m_ZoneRandomBtn;
	private ButtonWidget m_PresetRandomBtn;
	private TextWidget m_VoteTimer;
	private TextWidget m_SelectionText;
	private TextWidget m_ZoneHeader;
	private TextWidget m_PresetHeader;
	private ButtonWidget m_BtnClose;

	private int m_SelZone = -1;
	private int m_SelPreset = -1;

	override Widget Init()
	{
		layoutRoot = GetGame().GetWorkspace().CreateWidgets("SentinelDM/layouts/dm_vote.layout");

		m_VoteTimer = TextWidget.Cast(layoutRoot.FindAnyWidget("VoteTimer"));
		m_SelectionText = TextWidget.Cast(layoutRoot.FindAnyWidget("SelectionText"));
		m_ZoneHeader = TextWidget.Cast(layoutRoot.FindAnyWidget("ZoneHeader"));
		m_PresetHeader = TextWidget.Cast(layoutRoot.FindAnyWidget("PresetHeader"));
		m_ZoneRandomBtn = ButtonWidget.Cast(layoutRoot.FindAnyWidget("zbtn_rand"));
		m_PresetRandomBtn = ButtonWidget.Cast(layoutRoot.FindAnyWidget("pbtn_rand"));
		m_BtnClose = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnClose"));

		for (int slotIdx = 0; slotIdx < DmVoteService.MAX_LISTED_OPTIONS; slotIdx++)
		{
			m_ZoneButtons.Insert(ButtonWidget.Cast(layoutRoot.FindAnyWidget("zbtn_" + slotIdx.ToString())));
			m_PresetButtons.Insert(ButtonWidget.Cast(layoutRoot.FindAnyWidget("pbtn_" + slotIdx.ToString())));
		}

		RefreshOptions();
		return layoutRoot;
	}

	override bool UseMouse() { return true; }
	override bool UseKeyboard() { return true; }

	// Grab game focus while open: the cursor must drive the UI, not the
	// character/camera behind it. Released symmetrically on hide.
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

	void RefreshOptions()
	{
		DmClientState state = DmClientState.GetInstance();
		m_SelZone = -1;
		m_SelPreset = -1;

		for (int slotIdx = 0; slotIdx < DmVoteService.MAX_LISTED_OPTIONS; slotIdx++)
		{
			ButtonWidget zoneBtn = m_ZoneButtons[slotIdx];
			if (zoneBtn)
			{
				if (slotIdx < state.m_ZoneOptions.Count())
				{
					zoneBtn.SetText(state.m_ZoneOptions[slotIdx]);
					zoneBtn.Show(true);
				}
				else
				{
					zoneBtn.Show(false);
				}
			}

			ButtonWidget presetBtn = m_PresetButtons[slotIdx];
			if (presetBtn)
			{
				if (slotIdx < state.m_PresetOptions.Count())
				{
					presetBtn.SetText(state.m_PresetOptions[slotIdx]);
					presetBtn.Show(true);
				}
				else
				{
					presetBtn.Show(false);
				}
			}
		}

		// The Random pick is pointless with fewer than two real options.
		bool randomAllowed = state.IsRandomChoiceAllowed();
		int zoneCount = state.m_ZoneOptions.Count();
		int presetCount = state.m_PresetOptions.Count();
		if (m_ZoneRandomBtn) m_ZoneRandomBtn.Show(randomAllowed && zoneCount >= 2);
		if (m_PresetRandomBtn) m_PresetRandomBtn.Show(randomAllowed && presetCount >= 2);
		SetHeader(m_ZoneHeader, "ARENA", zoneCount);
		SetHeader(m_PresetHeader, "WEAPONS", presetCount);
		UpdateSelectionText();
	}

	// No options = the server rolls this column; say so where the buttons
	// would have been.
	private static void SetHeader(TextWidget header, string label, int optionCount)
	{
		if (!header) return;
		if (optionCount == 0)
		{
			header.SetText(label + " - RANDOM");
		}
		else
		{
			header.SetText(label);
		}
	}

	// Called from the HUD controller's tick while open. Uses the SAME synced
	// phase deadline as the HUD timer - two clocks from two RPCs drifted by
	// their delivery gap and disagreed on screen.
	void UpdateTimer()
	{
		if (!m_VoteTimer) return;
		DmClientState state = DmClientState.GetInstance();
		int remainInt = state.GetPhaseRemaining();
		m_VoteTimer.SetText("Voting closes in " + remainInt.ToString() + "s");
	}

	// Selection label: a real option's name, "Random" for the extra slot at
	// index == count, "-" for nothing yet.
	private static string PickLabel(array<string> options, int sel)
	{
		if (sel < 0) return "-";
		if (sel < options.Count()) return options[sel];
		if (sel == options.Count()) return "Random";
		return "-";
	}

	private void UpdateSelectionText()
	{
		if (!m_SelectionText) return;
		DmClientState state = DmClientState.GetInstance();

		bool zoneVote = state.m_ZoneOptions.Count() > 0;
		bool presetVote = state.m_PresetOptions.Count() > 0;
		string zonePick = PickLabel(state.m_ZoneOptions, m_SelZone);
		string presetPick = PickLabel(state.m_PresetOptions, m_SelPreset);

		if (m_SelZone < 0 && m_SelPreset < 0)
		{
			if (zoneVote && presetVote)
			{
				m_SelectionText.SetText("Click an arena and a weapon set to vote");
			}
			else if (zoneVote)
			{
				m_SelectionText.SetText("Click an arena to vote - weapons are random this round");
			}
			else if (presetVote)
			{
				m_SelectionText.SetText("Click a weapon set to vote - the arena is random this round");
			}
			else
			{
				m_SelectionText.SetText("Arena and weapons are random this round");
			}
		}
		else if (zoneVote && presetVote)
		{
			m_SelectionText.SetText("Your vote: " + zonePick + " / " + presetPick);
		}
		else if (zoneVote)
		{
			m_SelectionText.SetText("Your vote: " + zonePick);
		}
		else
		{
			m_SelectionText.SetText("Your vote: " + presetPick);
		}
	}

	private void SendVote()
	{
		Man ownPlayer = GetGame().GetPlayer();
		if (!ownPlayer) return;
		Param2<int, int> votePayload = new Param2<int, int>(m_SelZone, m_SelPreset);
		GetGame().RPCSingleParam(ownPlayer, DmRpc.VOTE_CAST, votePayload, true);
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (w == m_BtnClose)
		{
			Close();
			return true;
		}

		DmClientState state = DmClientState.GetInstance();
		if (m_ZoneRandomBtn && w == m_ZoneRandomBtn)
		{
			m_SelZone = state.m_ZoneOptions.Count();
			UpdateSelectionText();
			SendVote();
			return true;
		}
		if (m_PresetRandomBtn && w == m_PresetRandomBtn)
		{
			m_SelPreset = state.m_PresetOptions.Count();
			UpdateSelectionText();
			SendVote();
			return true;
		}

		for (int slotIdx = 0; slotIdx < DmVoteService.MAX_LISTED_OPTIONS; slotIdx++)
		{
			if (w == m_ZoneButtons[slotIdx])
			{
				m_SelZone = slotIdx;
				UpdateSelectionText();
				SendVote();
				return true;
			}
			if (w == m_PresetButtons[slotIdx])
			{
				m_SelPreset = slotIdx;
				UpdateSelectionText();
				SendVote();
				return true;
			}
		}

		return super.OnClick(w, x, y, button);
	}
}
