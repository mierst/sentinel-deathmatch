// Fixed-size vote controls share the leaderboard's native surfaces and theme.
// The existing HUD timer handles deadline, late theme delivery and resizing.
class DmVoteMenu extends UIScriptedMenu
{
	private ref array<ButtonWidget> m_ZoneButtons = new array<ButtonWidget>;
	private ref array<ButtonWidget> m_PresetButtons = new array<ButtonWidget>;
	private ButtonWidget m_ZoneRandomBtn;
	private ButtonWidget m_PresetRandomBtn;
	private ButtonWidget m_BtnClose;
	private ref DmLeaderboardTheme m_Theme;
	private int m_SeenThemeSeq = -1;
	private int m_SeenVoteSeq = -1;
	private int m_SelZone = -1;
	private int m_SelPreset = -1;
	private int m_ZoneOffset;
	private int m_PresetOffset;
	private int m_ScreenW;
	private int m_ScreenH;
	private float m_Scale = 1;
	private float m_TextScale = 1;
	private bool m_LogoLoaded;
	private Widget m_HoveredButton;

	override Widget Init()
	{
		m_HoveredButton = null;
		m_ZoneButtons.Clear();
		m_PresetButtons.Clear();
		if (layoutRoot) layoutRoot.Unlink();
		layoutRoot = GetGame().GetWorkspace().CreateWidgets("SentinelDM/layouts/dm_vote.layout");
		m_ZoneRandomBtn = ButtonWidget.Cast(layoutRoot.FindAnyWidget("zbtn_rand"));
		m_PresetRandomBtn = ButtonWidget.Cast(layoutRoot.FindAnyWidget("pbtn_rand"));
		m_BtnClose = ButtonWidget.Cast(layoutRoot.FindAnyWidget("BtnClose"));
		for (int slotIdx = 0; slotIdx < 8; slotIdx++)
		{
			m_ZoneButtons.Insert(ButtonWidget.Cast(layoutRoot.FindAnyWidget("zbtn_" + slotIdx.ToString())));
			m_PresetButtons.Insert(ButtonWidget.Cast(layoutRoot.FindAnyWidget("pbtn_" + slotIdx.ToString())));
		}
		LoadSolidImages(layoutRoot);
		DmClientState initState = DmClientState.GetInstance();
		m_Theme = initState.m_VoteTheme;
		m_SeenThemeSeq = initState.m_VoteThemeSeq;
		ApplyTheme();
		RefreshOptions();
		return layoutRoot;
	}

	override bool UseMouse() { return true; }
	override bool UseKeyboard() { return true; }

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
		m_HoveredButton = null;
		GetGame().GetInput().ResetGameFocus();
		GetGame().GetUIManager().ShowUICursor(false);
	}

	void RefreshOptions()
	{
		DmClientState state = DmClientState.GetInstance();
		m_SeenVoteSeq = state.m_VoteSeq;
		m_SelZone = -1;
		m_SelPreset = -1;
		m_ZoneOffset = 0;
		m_PresetOffset = 0;
		Reflow();
	}

	private void LoadSolidImages(Widget parentWidget)
	{
		ImageWidget fillWidget = ImageWidget.Cast(parentWidget);
		if (fillWidget && parentWidget.GetName() != "Logo")
		{
			fillWidget.SetFlags(WidgetFlags.STRETCH | WidgetFlags.BLEND | WidgetFlags.SOURCEALPHA);
			fillWidget.LoadImageFile(0, "SentinelDM/graphics/leaderboard/solid.paa");
			fillWidget.SetImage(0);
		}
		Widget childWidget = parentWidget.GetChildren();
		while (childWidget)
		{
			LoadSolidImages(childWidget);
			childWidget = childWidget.GetSibling();
		}
	}

	private void ApplyTheme()
	{
		if (!m_Theme) m_Theme = new DmLeaderboardTheme();
		m_Theme.Validate();
		layoutRoot.FindAnyWidget("Backdrop").SetColor(m_Theme.Surface());
		layoutRoot.FindAnyWidget("Accent").SetColor(m_Theme.Accent());
		layoutRoot.FindAnyWidget("HeaderBackground").SetColor(ARGB(255, 23, 26, 33));
		layoutRoot.FindAnyWidget("SelectionBackground").SetColor(ARGB(255, 15, 18, 24));
		ImageWidget logoWidget = ImageWidget.Cast(layoutRoot.FindAnyWidget("Logo"));
		logoWidget.SetFlags(WidgetFlags.STRETCH | WidgetFlags.BLEND | WidgetFlags.SOURCEALPHA);
		m_LogoLoaded = false;
		if (m_Theme.ShowLogo && m_Theme.LogoPath != "") m_LogoLoaded = logoWidget.LoadImageFile(0, m_Theme.LogoPath);
		logoWidget.Show(m_LogoLoaded);
		if (m_LogoLoaded) logoWidget.SetImage(0);
	}

	private void Box(string name, float x, float y, float width, float height)
	{
		Widget target = layoutRoot.FindAnyWidget(name);
		if (!target) return;
		target.SetPos(x, y);
		target.SetSize(width, height);
	}

	private void TextBox(string name, float x, float y, float width, float height, int pixels)
	{
		Box(name, x, y, width, height);
		TextWidget label = TextWidget.Cast(layoutRoot.FindAnyWidget(name));
		if (label) label.SetTextExactSize(pixels * m_TextScale);
	}

	private void FitLabel(string name, string value)
	{
		TextWidget label = TextWidget.Cast(layoutRoot.FindAnyWidget(name));
		if (!label) return;
		float width;
		float height;
		label.GetSize(width, height);
		label.SetText(value);
		int measuredWidth;
		int measuredHeight;
		label.GetTextSize(measuredWidth, measuredHeight);
		int trimLength = value.Length();
		while (measuredWidth > width && trimLength > 3)
		{
			trimLength = trimLength - 1;
			label.SetText(value.Substring(0, trimLength) + "...");
			label.GetTextSize(measuredWidth, measuredHeight);
		}
	}

	private void ButtonBox(string name, float x, float y, float width, float height)
	{
		Box(name, x, y, width, height);
		Box(name + "Fill", 0, 0, width, height);
		Box(name + "Accent", 0, 0, 3 * m_Scale, height);
		TextBox(name + "Label", 16 * m_Scale, 0, width - 32 * m_Scale, height, 19);
	}

	private void ScrollButtonBox(string name, float x, float y, float width, float height)
	{
		Box(name, x, y, width, height);
		Box(name + "Fill", 0, 0, width, height);
		TextBox(name + "Label", 0, 0, width, height, 18);
		layoutRoot.FindAnyWidget(name + "Accent").Show(false);
		PaintButtonFill(layoutRoot.FindAnyWidget(name), false);
	}

	private void ScrollGeometry(string column, float x, float y, float height)
	{
		float control = 26 * m_Scale;
		float gap = 3 * m_Scale;
		ScrollButtonBox(column + "Up", x, y, 24 * m_Scale, control);
		ScrollButtonBox(column + "Track", x, y + control + gap, 24 * m_Scale, height - 2 * (control + gap));
		ScrollButtonBox(column + "Down", x, y + height - control, 24 * m_Scale, control);
	}

	private void RefreshScroll(string column, int count, int offset)
	{
		bool scrolling = count > 8;
		layoutRoot.FindAnyWidget(column + "Up").Show(scrolling);
		layoutRoot.FindAnyWidget(column + "Down").Show(scrolling);
		layoutRoot.FindAnyWidget(column + "Track").Show(scrolling);
		layoutRoot.FindAnyWidget(column + "Thumb").Show(scrolling);
		int first = 0;
		if (count > 0) first = offset + 1;
		int last = Math.Min(count, offset + 8);
		FitLabel(column + "Range", first.ToString() + " - " + last.ToString() + " OF " + count.ToString());
		if (!scrolling) return;
		float trackX;
		float trackY;
		float trackWidth;
		float trackHeight;
		Widget track = layoutRoot.FindAnyWidget(column + "Track");
		track.GetPos(trackX, trackY);
		track.GetSize(trackWidth, trackHeight);
		float thumbHeight = Math.Max(24 * m_Scale, trackHeight * 8 / count);
		float fraction = offset / (count - 8.0);
		Box(column + "Thumb", trackX + 4 * m_Scale, trackY + fraction * (trackHeight - thumbHeight), trackWidth - 8 * m_Scale, thumbHeight);
	}

	private void ScrollColumn(bool zone, int delta)
	{
		DmClientState state = DmClientState.GetInstance();
		if (zone)
		{
			int nextZone = Math.Clamp(m_ZoneOffset + delta, 0, Math.Max(0, state.m_ZoneOptions.Count() - 8));
			if (nextZone == m_ZoneOffset) return;
			m_ZoneOffset = nextZone;
		}
		else
		{
			int nextPreset = Math.Clamp(m_PresetOffset + delta, 0, Math.Max(0, state.m_PresetOptions.Count() - 8));
			if (nextPreset == m_PresetOffset) return;
			m_PresetOffset = nextPreset;
		}
		UpdateSelectionText();
	}

	private void JumpTrack(bool zone, Widget track, int y)
	{
		DmClientState state = DmClientState.GetInstance();
		int count = state.m_PresetOptions.Count();
		int oldOffset = m_PresetOffset;
		if (zone)
		{
			count = state.m_ZoneOptions.Count();
			oldOffset = m_ZoneOffset;
		}
		if (count <= 8) return;
		float trackX;
		float trackY;
		float trackWidth;
		float trackHeight;
		track.GetScreenPos(trackX, trackY);
		track.GetSize(trackWidth, trackHeight);
		float fraction = Math.Clamp((y - trackY) / Math.Max(1, trackHeight - 1), 0, 1);
		// Exact geometry can be fractional while pointer coordinates are whole pixels.
		if (y <= Math.Ceil(trackY)) fraction = 0;
		else if (y >= Math.Floor(trackY + trackHeight) - 1) fraction = 1;
		int targetOffset = Math.Floor(fraction * (count - 8) + 0.5);
		ScrollColumn(zone, targetOffset - oldOffset);
	}

	private bool HandleScrollClick(Widget w, int y)
	{
		string name = w.GetName();
		if (name == "ZoneUp") ScrollColumn(true, -8);
		else if (name == "ZoneDown") ScrollColumn(true, 8);
		else if (name == "PresetUp") ScrollColumn(false, -8);
		else if (name == "PresetDown") ScrollColumn(false, 8);
		else if (name == "ZoneTrack") JumpTrack(true, w, y);
		else if (name == "PresetTrack") JumpTrack(false, w, y);
		else return false;
		return true;
	}

	private bool InsideColumn(string name, int x, int y)
	{
		Widget area = layoutRoot.FindAnyWidget(name);
		float areaX;
		float areaY;
		float areaWidth;
		float areaHeight;
		area.GetScreenPos(areaX, areaY);
		area.GetSize(areaWidth, areaHeight);
		return x >= areaX && x < areaX + areaWidth && y >= areaY && y < areaY + areaHeight;
	}

	override bool OnMouseWheel(Widget w, int x, int y, int wheel)
	{
		int delta = 0;
		if (wheel > 0) delta = -3;
		else if (wheel < 0) delta = 3;
		if (InsideColumn("ZoneViewport", x, y)) ScrollColumn(true, delta);
		else if (InsideColumn("PresetViewport", x, y)) ScrollColumn(false, delta);
		else return super.OnMouseWheel(w, x, y, wheel);
		return true;
	}

	private void Reflow()
	{
		GetScreenSize(m_ScreenW, m_ScreenH);
		m_Scale = Math.Clamp(m_ScreenH / 1080.0, 0.85, 1.30);
		m_TextScale = m_Scale * 1080.0 / Math.Max(1, m_ScreenH);
		float width = Math.Min(1000 * m_Scale, m_ScreenW - 64);
		float height = Math.Min(820 * m_Scale, m_ScreenH - 60);
		float pad = 26 * m_Scale;
		float columnWidth = (width - 3 * pad) / 2;
		float rightX = 2 * pad + columnWidth;
		float bodyY = 190 * m_Scale;
		float summaryY = height - 156 * m_Scale;
		float rowStep = (summaryY - bodyY - 12 * m_Scale) / 9;
		float rowHeight = rowStep - 5 * m_Scale;
		layoutRoot.SetPos((m_ScreenW - width) / 2, (m_ScreenH - height) / 2);
		layoutRoot.SetSize(width, height);
		Box("Backdrop", 0, 0, width, height);
		Box("Accent", 0, 0, width, 3 * m_Scale);
		Box("Logo", pad, 23 * m_Scale, 38 * m_Scale, 38 * m_Scale);
		float brandX = pad;
		if (m_LogoLoaded) brandX = 78 * m_Scale;
		TextBox("Brand", brandX, 20 * m_Scale, width - brandX - pad, 42 * m_Scale, 25);
		TextBox("Subtitle", pad, 65 * m_Scale, width - 2 * pad, 25 * m_Scale, 16);
		TextBox("Title", pad, 100 * m_Scale, width - 360 * m_Scale, 46 * m_Scale, 30);
		TextBox("VoteTimer", width - 300 * m_Scale, 102 * m_Scale, 274 * m_Scale, 42 * m_Scale, 22);
		Box("HeaderBackground", 0, 152 * m_Scale, width, 32 * m_Scale);
		TextBox("ZoneHeader", pad, 152 * m_Scale, columnWidth - 150 * m_Scale, 32 * m_Scale, 17);
		TextBox("PresetHeader", rightX, 152 * m_Scale, columnWidth - 150 * m_Scale, 32 * m_Scale, 17);
		TextBox("ZoneEmpty", pad, bodyY, columnWidth, 48 * m_Scale, 18);
		TextBox("PresetEmpty", rightX, bodyY, columnWidth, 48 * m_Scale, 18);
		TextBox("ZoneRange", pad + columnWidth - 140 * m_Scale, 152 * m_Scale, 140 * m_Scale, 32 * m_Scale, 15);
		TextBox("PresetRange", rightX + columnWidth - 140 * m_Scale, 152 * m_Scale, 140 * m_Scale, 32 * m_Scale, 15);
		// Hit areas include unused slots, so the wheel works between rows too.
		Box("ZoneViewport", pad, bodyY, columnWidth, 9 * rowStep);
		Box("PresetViewport", rightX, bodyY, columnWidth, 9 * rowStep);
		DmClientState state = DmClientState.GetInstance();
		float zoneButtonWidth = columnWidth;
		float presetButtonWidth = columnWidth;
		if (state.m_ZoneOptions.Count() > 8) zoneButtonWidth = columnWidth - 32 * m_Scale;
		if (state.m_PresetOptions.Count() > 8) presetButtonWidth = columnWidth - 32 * m_Scale;
		for (int slotIdx = 0; slotIdx < 8; slotIdx++)
		{
			ButtonBox("zbtn_" + slotIdx.ToString(), pad, bodyY + slotIdx * rowStep, zoneButtonWidth, rowHeight);
			ButtonBox("pbtn_" + slotIdx.ToString(), rightX, bodyY + slotIdx * rowStep, presetButtonWidth, rowHeight);
		}
		// Random is outside the eight-slot viewport and never scrolls away.
		ButtonBox("zbtn_rand", pad, bodyY + 8 * rowStep, columnWidth, rowHeight);
		ButtonBox("pbtn_rand", rightX, bodyY + 8 * rowStep, columnWidth, rowHeight);
		ScrollGeometry("Zone", pad + columnWidth - 24 * m_Scale, bodyY, 8 * rowStep - 5 * m_Scale);
		ScrollGeometry("Preset", rightX + columnWidth - 24 * m_Scale, bodyY, 8 * rowStep - 5 * m_Scale);
		Box("SelectionBackground", 0, summaryY, width, 62 * m_Scale);
		TextBox("SelectionText", pad, summaryY + 4 * m_Scale, width - 2 * pad, 22 * m_Scale, 15);
		TextBox("ZonePick", pad, summaryY + 27 * m_Scale, columnWidth, 28 * m_Scale, 18);
		TextBox("PresetPick", rightX, summaryY + 27 * m_Scale, columnWidth, 28 * m_Scale, 18);
		TextBox("Hint", pad, summaryY + 73 * m_Scale, width - 240 * m_Scale, 34 * m_Scale, 16);
		ButtonBox("BtnClose", width - 198 * m_Scale, summaryY + 73 * m_Scale, 172 * m_Scale, 36 * m_Scale);
		layoutRoot.FindAnyWidget("BtnCloseAccent").Show(false);
		PaintButtonFill(m_BtnClose, false);
		Box("FooterLine", pad, height - 34 * m_Scale, width - 2 * pad, 1);
		TextBox("Footer", pad, height - 30 * m_Scale, width - 350 * m_Scale, 24 * m_Scale, 15);
		Box("AttributionBackground", width - 322 * m_Scale, height - 31 * m_Scale, 296 * m_Scale, 26 * m_Scale);
		TextBox("Attribution", width - 314 * m_Scale, height - 30 * m_Scale, 288 * m_Scale, 24 * m_Scale, 14);
		FitLabel("Brand", m_Theme.BrandName);
		FitLabel("Subtitle", m_Theme.Subtitle);
		FitLabel("Footer", m_Theme.Footer);
		FitLabel("Title", "VOTE - NEXT ROUND");
		FitLabel("Attribution", "Powered by Sentinel Deathmatch");
		FitLabel("ZoneEmpty", "Server chooses a random arena");
		FitLabel("PresetEmpty", "Server chooses random weapons");
		FitLabel("Hint", "Click a choice to vote. Click again to change.");
		FitLabel("BtnCloseLabel", "CLOSE");
		UpdateSelectionText();
		UpdateDeadline();
	}

	void UpdateTimer()
	{
		if (!layoutRoot) return;
		DmClientState state = DmClientState.GetInstance();
		bool themeChanged = state.m_VoteThemeSeq != m_SeenThemeSeq;
		if (themeChanged)
		{
			m_SeenThemeSeq = state.m_VoteThemeSeq;
			m_Theme = state.m_VoteTheme;
			ApplyTheme();
		}
		// A complete option snapshot can arrive after the player opens with B.
		if (state.m_VoteSeq != m_SeenVoteSeq)
		{
			RefreshOptions();
			return;
		}
		int screenWidth;
		int screenHeight;
		GetScreenSize(screenWidth, screenHeight);
		if (themeChanged || screenWidth != m_ScreenW || screenHeight != m_ScreenH) Reflow();
		else UpdateDeadline();
	}

	private void UpdateDeadline()
	{
		DmClientState state = DmClientState.GetInstance();
		int remaining = state.GetPhaseRemaining();
		FitLabel("VoteTimer", "CLOSES IN " + remaining.ToString() + "s");
	}

	private static string PickLabel(array<string> options, int selection)
	{
		if (options.Count() == 0) return "Random (server)";
		if (selection < 0) return "Not selected";
		if (selection < options.Count()) return options[selection];
		if (selection == options.Count()) return "Random";
		return "Not selected";
	}

	private void BindChoice(string name, string label, bool visible, bool selected)
	{
		layoutRoot.FindAnyWidget(name).Show(visible);
		if (!visible) return;
		string displayLabel = label;
		if (selected) displayLabel = "> " + label;
		FitLabel(name + "Label", displayLabel);
		PaintButtonFill(layoutRoot.FindAnyWidget(name), selected);
		layoutRoot.FindAnyWidget(name + "Accent").SetColor(m_Theme.Accent());
		layoutRoot.FindAnyWidget(name + "Accent").Show(selected);
	}

	private void PaintButtonFill(Widget button, bool selected)
	{
		if (!button || !m_Theme) return;
		Widget fill = button.FindAnyWidget(button.GetName() + "Fill");
		if (!fill) return;
		int hoverBoost = 0;
		if (button == m_HoveredButton) hoverBoost = 20;
		int fillColor = ARGB(255, 15 + hoverBoost, 18 + hoverBoost, 24 + hoverBoost);
		if (selected) fillColor = ARGB(255, 16 + m_Theme.AccentR / 9 + hoverBoost, 18 + m_Theme.AccentG / 9 + hoverBoost, 24 + m_Theme.AccentB / 9 + hoverBoost);
		fill.SetColor(fillColor);
	}

	private bool IsSelectedButton(Widget button)
	{
		DmClientState state = DmClientState.GetInstance();
		if (button == m_ZoneRandomBtn) return m_SelZone == state.m_ZoneOptions.Count();
		if (button == m_PresetRandomBtn) return m_SelPreset == state.m_PresetOptions.Count();
		for (int selectedIndex = 0; selectedIndex < 8; selectedIndex++)
		{
			if (button == m_ZoneButtons[selectedIndex]) return m_SelZone == m_ZoneOffset + selectedIndex;
			if (button == m_PresetButtons[selectedIndex]) return m_SelPreset == m_PresetOffset + selectedIndex;
		}
		return false;
	}

	// Pointer events repaint at most two fills; text fitting stays on actual
	// selection, theme and geometry changes, not mouse movement.
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		if (!ButtonWidget.Cast(w) || !w.FindAnyWidget(w.GetName() + "Fill")) return super.OnMouseEnter(w, x, y);
		Widget previousButton = m_HoveredButton;
		m_HoveredButton = w;
		if (previousButton && previousButton != w) PaintButtonFill(previousButton, IsSelectedButton(previousButton));
		PaintButtonFill(w, IsSelectedButton(w));
		return true;
	}

	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (w != m_HoveredButton) return super.OnMouseLeave(w, enterW, x, y);
		m_HoveredButton = null;
		PaintButtonFill(w, IsSelectedButton(w));
		return true;
	}

	private void UpdateSelectionText()
	{
		DmClientState state = DmClientState.GetInstance();
		int zoneCount = state.m_ZoneOptions.Count();
		int presetCount = state.m_PresetOptions.Count();
		m_ZoneOffset = Math.Clamp(m_ZoneOffset, 0, Math.Max(0, zoneCount - 8));
		m_PresetOffset = Math.Clamp(m_PresetOffset, 0, Math.Max(0, presetCount - 8));
		for (int slotIdx = 0; slotIdx < 8; slotIdx++)
		{
			string zoneLabel = "";
			string presetLabel = "";
			int zoneIndex = m_ZoneOffset + slotIdx;
			int presetIndex = m_PresetOffset + slotIdx;
			if (zoneIndex < zoneCount) zoneLabel = state.m_ZoneOptions[zoneIndex];
			if (presetIndex < presetCount) presetLabel = state.m_PresetOptions[presetIndex];
			BindChoice("zbtn_" + slotIdx.ToString(), zoneLabel, zoneIndex < zoneCount, m_SelZone == zoneIndex);
			BindChoice("pbtn_" + slotIdx.ToString(), presetLabel, presetIndex < presetCount, m_SelPreset == presetIndex);
		}
		RefreshScroll("Zone", zoneCount, m_ZoneOffset);
		RefreshScroll("Preset", presetCount, m_PresetOffset);
		bool randomAllowed = state.IsRandomChoiceAllowed();
		BindChoice("zbtn_rand", "RANDOM", randomAllowed && zoneCount >= 2, m_SelZone == zoneCount);
		BindChoice("pbtn_rand", "RANDOM", randomAllowed && presetCount >= 2, m_SelPreset == presetCount);
		layoutRoot.FindAnyWidget("ZoneEmpty").Show(zoneCount == 0);
		layoutRoot.FindAnyWidget("PresetEmpty").Show(presetCount == 0);
		FitLabel("ZonePick", "Arena: " + PickLabel(state.m_ZoneOptions, m_SelZone));
		FitLabel("PresetPick", "Weapons: " + PickLabel(state.m_PresetOptions, m_SelPreset));
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
		if (button != MouseState.LEFT) return super.OnClick(w, x, y, button);
		if (HandleScrollClick(w, y)) return true;
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

		for (int slotIdx = 0; slotIdx < 8; slotIdx++)
		{
			if (w == m_ZoneButtons[slotIdx])
			{
				m_SelZone = m_ZoneOffset + slotIdx;
				UpdateSelectionText();
				SendVote();
				return true;
			}
			if (w == m_PresetButtons[slotIdx])
			{
				m_SelPreset = m_PresetOffset + slotIdx;
				UpdateSelectionText();
				SendVote();
				return true;
			}
		}

		return super.OnClick(w, x, y, button);
	}
}
