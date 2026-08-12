// Vote screen. Opened by the HUD controller when a vote window arrives; one
// click per column casts (re-click to change); every cast sends the current
// selection pair - the server rate-limits and bounds-checks.
class DmVoteMenu extends UIScriptedMenu
{
	private ref array<ButtonWidget> m_ZoneButtons = new array<ButtonWidget>;
	private ref array<ButtonWidget> m_PresetButtons = new array<ButtonWidget>;
	private TextWidget m_VoteTimer;
	private TextWidget m_SelectionText;
	private ButtonWidget m_BtnClose;

	private int m_SelZone = -1;
	private int m_SelPreset = -1;

	override Widget Init()
	{
		layoutRoot = GetGame().GetWorkspace().CreateWidgets("SentinelDM/layouts/dm_vote.layout");

		m_VoteTimer = TextWidget.Cast(layoutRoot.FindAnyWidget("VoteTimer"));
		m_SelectionText = TextWidget.Cast(layoutRoot.FindAnyWidget("SelectionText"));
		m_BtnClose = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnClose"));

		for (int slotIdx = 0; slotIdx < 8; slotIdx++)
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

		for (int slotIdx = 0; slotIdx < 8; slotIdx++)
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
		UpdateSelectionText();
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

	private void UpdateSelectionText()
	{
		if (!m_SelectionText) return;
		DmClientState state = DmClientState.GetInstance();

		string zonePick = "-";
		if (m_SelZone >= 0 && m_SelZone < state.m_ZoneOptions.Count()) zonePick = state.m_ZoneOptions[m_SelZone];
		string presetPick = "-";
		if (m_SelPreset >= 0 && m_SelPreset < state.m_PresetOptions.Count()) presetPick = state.m_PresetOptions[m_SelPreset];

		if (m_SelZone < 0 && m_SelPreset < 0)
		{
			m_SelectionText.SetText("Click an arena and a weapon set to vote");
		}
		else
		{
			m_SelectionText.SetText("Your vote: " + zonePick + " / " + presetPick);
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

		for (int slotIdx = 0; slotIdx < 8; slotIdx++)
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
