// Chat history hold (config ChatHistoryOnOpen, server-decided, off by
// default). Vanilla keeps a ring of 12 ChatLine widgets, fades each one out
// 30 s after it arrives, and opening the chat box shows nothing of them.
// With the switch on, opening the box re-shows every line that still holds
// text and pauses its fade; closing the box lets them fade again.
//
// Compatibility posture (the point of this file's shape):
//  - Additive only. No override of Init/Set/Clear that changes behaviour,
//    no layout swapped, no widget Unlinked, no vanilla constant redefined.
//    The only vanilla members touched are the ones every chat variant
//    keeps: the line's root widget and its two timers.
//  - Every new member is m_Dm-/Dm-prefixed, so it cannot collide with
//    another mod's additions on the same class (a same-name different-type
//    member is a World-module compile failure that blames the OTHER mod).
//  - Compiled out under DayZ Expansion Chat and LBmaster Groups: both
//    replace these widgets with their own layouts and already ship a
//    scrollable history, so re-showing vanilla lines there would be wrong.
#ifndef EXPANSIONMODCHAT
#ifndef LBmaster_Groups

modded class ChatLine
{
	bool m_DmHistoryHeld = false;
	bool m_DmTimeoutWasRunning = false;
	// TextWidget has no getter in Enforce, so "this slot holds a line"
	// is tracked here: set by Set(), dropped by Clear() (vanilla never
	// re-shows a cleared line either, so this mirrors its visual state).
	bool m_DmHasLine = false;

	void DmHistoryHold()
	{
		if (m_DmHistoryHeld) return;
		if (!m_RootWidget) return;
		if (!m_DmHasLine) return; // slot never used, or cleared on respawn
		m_DmHistoryHeld = true;
		m_DmTimeoutWasRunning = m_TimeoutTimer.IsRunning();
		m_TimeoutTimer.Pause();
		m_FadeTimer.Stop();
		m_RootWidget.SetAlpha(1.0);
		m_RootWidget.Show(true);
	}

	void DmHistoryRelease()
	{
		if (!m_DmHistoryHeld) return;
		m_DmHistoryHeld = false;
		if (!m_RootWidget) return;
		if (m_DmTimeoutWasRunning)
		{
			// Still inside its 30 s: vanilla's own fade fires when the
			// remaining time elapses.
			m_TimeoutTimer.Continue();
			return;
		}
		m_FadeTimer.FadeOut(m_RootWidget, FADE_OUT_DURATION);
	}

	// A line rewritten while held is vanilla again (fresh timers); a
	// cleared line has nothing to release.
	override void Set(ChatMessageEventParams params)
	{
		super.Set(params);
		m_DmHistoryHeld = false;
		m_DmHasLine = true;
	}

	override void Clear()
	{
		super.Clear();
		m_DmHistoryHeld = false;
		m_DmHasLine = false;
	}
}

modded class Chat
{
	void DmHistoryHold(bool hold)
	{
		if (!m_Lines) return;
		for (int lineIdx = 0; lineIdx < m_Lines.Count(); lineIdx++)
		{
			ChatLine line = m_Lines[lineIdx];
			if (!line) continue;
			if (hold)
			{
				line.DmHistoryHold();
			}
			else
			{
				line.DmHistoryRelease();
			}
		}
	}
}

// Entry point for the ChatInputMenu show/hide overrides (DmChatCommands.c).
// Release always runs (a no-op when nothing is held); hold only when the
// server switched the feature on for this client.
class DmChatHistory
{
	static void Hold(bool hold)
	{
		if (hold && !DmClientState.GetInstance().IsChatHistoryOnOpen()) return;
		MissionGameplay mission = MissionGameplay.Cast(GetGame().GetMission());
		if (!mission || !mission.m_Chat) return;
		mission.m_Chat.DmHistoryHold(hold);
	}
}

#endif
#endif
