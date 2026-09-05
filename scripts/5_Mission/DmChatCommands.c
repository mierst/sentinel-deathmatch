// Chat-command capture. DayZ has no server-side chat hook, so commands are
// intercepted in the client's chat input box before they ever become chat:
// a recognized command is swallowed (never broadcast) and sent to the server
// as an RPC instead.
//
// Chain-link rule: this is the mod's ONE modded block for ChatInputMenu.
modded class ChatInputMenu
{
	override bool OnChange(Widget w, int x, int y, bool finished)
	{
		if (finished && m_edit_box)
		{
			// Trim() returns a copy; ToLower() mutates in place.
			string lowered = m_edit_box.GetText();
			lowered = lowered.Trim();
			lowered.ToLower();
			if (lowered == "/mapvote")
			{
				// Swallow the text so super's ChatPlayer never sees it.
				m_edit_box.SetText("");
				Man ownPlayer = GetGame().GetPlayer();
				if (ownPlayer)
				{
					GetGame().RPCSingleParam(ownPlayer, DmRpc.MAP_VOTE, new Param1<int>(1), true);
				}
			}
		}
		return super.OnChange(w, x, y, finished);
	}

	// Chat history hold (config ChatHistoryOnOpen): open box = the last
	// lines stay visible; close = they resume fading. Additive only - super
	// runs in both, no widget or layout is touched here. Compiled out where
	// a chat-replacing mod owns these widgets (see DmChatHistory.c).
#ifndef EXPANSIONMODCHAT
#ifndef LBmaster_Groups
	override void OnShow()
	{
		super.OnShow();
		DmChatHistory.Hold(true);
	}

	override void OnHide()
	{
		DmChatHistory.Hold(false);
		super.OnHide();
	}
#endif
#endif
}
