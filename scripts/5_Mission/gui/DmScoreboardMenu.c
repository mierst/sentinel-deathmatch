// Native leaderboard view. The server returns at most 100 ordered rows; the
// menu keeps a fixed pool of 18 widgets and moves only inside that response
// until navigation crosses a page boundary.
class DmLeaderboardRowView
{
	Widget Root;
	TextWidget Rank;
	TextWidget Name;
	TextWidget Kills;
	TextWidget Deaths;
	TextWidget Streak;
	float NameWidth;

	void DmLeaderboardRowView(Widget parent)
	{
		Root = GetGame().GetWorkspace().CreateWidgets("SentinelDM/layouts/dm_scoreboard_row.layout", parent);
		Rank = TextWidget.Cast(Root.FindAnyWidget("Rank"));
		Name = TextWidget.Cast(Root.FindAnyWidget("Name"));
		Kills = TextWidget.Cast(Root.FindAnyWidget("Kills"));
		Deaths = TextWidget.Cast(Root.FindAnyWidget("Deaths"));
		Streak = TextWidget.Cast(Root.FindAnyWidget("Streak"));
	}

	void Geometry(float width, float height, float scale, float textScale)
	{
		Root.SetSize(width, height);
		Root.FindAnyWidget("RowBackground").SetSize(width, height);
		Root.FindAnyWidget("RowAccent").SetSize(3 * scale, height);
		Root.FindAnyWidget("RowLine").SetPos(24 * scale, height - 1);
		Root.FindAnyWidget("RowLine").SetSize(width - 48 * scale, 1);
		float statWidth = 88 * scale;
		float rightPad = 26 * scale;
		float streakWidth = 130 * scale;
		float streakX = width - rightPad - streakWidth;
		float deathsX = streakX - statWidth - 12 * scale;
		float killsX = deathsX - statWidth - 12 * scale;
		NameWidth = killsX - 114 * scale;
		Rank.SetPos(26 * scale, 0);
		Rank.SetSize(64 * scale, height);
		Name.SetPos(100 * scale, 0);
		Name.SetSize(NameWidth, height);
		Kills.SetPos(killsX, 0);
		Kills.SetSize(statWidth, height);
		Deaths.SetPos(deathsX, 0);
		Deaths.SetSize(statWidth, height);
		Streak.SetPos(streakX, 0);
		Streak.SetSize(streakWidth, height);
		Rank.SetTextExactSize(19 * textScale);
		Name.SetTextExactSize(22 * textScale);
		Kills.SetTextExactSize(22 * textScale);
		Deaths.SetTextExactSize(22 * textScale);
		Streak.SetTextExactSize(22 * textScale);
	}

	void Bind(DmLeaderboardRow entry, DmLeaderboardTheme theme, bool self, bool alternate)
	{
		Root.Show(true);
		Rank.SetText(entry.Rank.ToString());
		string rowLabel = entry.Name;
		if (self) rowLabel = rowLabel + "  [YOU]";
		Name.SetText(rowLabel);
		int rowTextWidth;
		int rowTextHeight;
		Name.GetTextSize(rowTextWidth, rowTextHeight);
		int rowTrimLength = rowLabel.Length();
		while (rowTextWidth > NameWidth && rowTrimLength > 3)
		{
			rowTrimLength = rowTrimLength - 1;
			Name.SetText(rowLabel.Substring(0, rowTrimLength) + "...");
			Name.GetTextSize(rowTextWidth, rowTextHeight);
		}
		Kills.SetText(entry.Kills.ToString());
		Deaths.SetText(entry.Deaths.ToString());
		Streak.SetText(entry.BestStreak.ToString());
		int rowColor = ARGB(255, 10, 12, 16);
		if (alternate) rowColor = ARGB(255, 15, 18, 24);
		if (self) rowColor = ARGB(255, 16 + theme.AccentR / 9, 18 + theme.AccentG / 9, 24 + theme.AccentB / 9);
		Root.FindAnyWidget("RowBackground").SetColor(rowColor);
		Root.FindAnyWidget("RowAccent").Show(self);
		Root.FindAnyWidget("RowAccent").SetColor(theme.Accent());
	}
}

class DmScoreboardMenu extends UIScriptedMenu
{
	static const int ROW_POOL_SIZE = 18;
	private ref array<ref DmLeaderboardRowView> m_Pool = new array<ref DmLeaderboardRowView>;
	private ref DmLeaderboardRowView m_Own;
	private ref array<ref DmLeaderboardRow> m_Rows = new array<ref DmLeaderboardRow>;
	private ref DmLeaderboardRow m_Self;
	private ref DmLeaderboardTheme m_Theme;
	private bool m_Session;
	private bool m_Loading;
	private bool m_HasResponse;
	private bool m_FindPending;
	private bool m_ApplyPendingLocal;
	private bool m_InputExcluded;
	private bool m_LogoLoaded;
	private int m_ResponseOffset;
	private int m_RequestedOffset;
	private int m_PendingLocalOffset;
	private int m_LocalOffset;
	private int m_Total;
	private int m_Visible = 12;
	private int m_SeenLeaderboardSeq = -1;
	private int m_SeenThemeSeq = -1;
	private int m_SeenScoreboardSeq = -1;
	private int m_SeenPhase = -1;
	private int m_ScreenW;
	private int m_ScreenH;
	private float m_Scale = 1;
	private float m_TextScale = 1;
	private float m_RowHeight = 38;
	private float m_BodyY;
	private float m_BodyHeight;
	private float m_Width;

	override Widget Init()
	{
		m_Pool.Clear();
		m_Own = null;
		m_Rows = new array<ref DmLeaderboardRow>;
		m_Self = null;
		m_Total = 0;
		m_ResponseOffset = 0;
		m_LocalOffset = 0;
		m_HasResponse = false;
		m_Loading = true;
		m_FindPending = false;
		m_ApplyPendingLocal = false;
		if (layoutRoot) layoutRoot.Unlink();
		layoutRoot = GetGame().GetWorkspace().CreateWidgets("SentinelDM/layouts/dm_scoreboard.layout");
		Widget rowHost = layoutRoot.FindAnyWidget("Rows");
		for (int poolIndex = 0; poolIndex < ROW_POOL_SIZE; poolIndex++)
		{
			m_Pool.Insert(new DmLeaderboardRowView(rowHost));
		}
		m_Own = new DmLeaderboardRowView(layoutRoot.FindAnyWidget("Own"));
		LoadSolidImages(layoutRoot);
		DmClientState initState = DmClientState.GetInstance();
		m_Theme = initState.m_LeaderboardTheme;
		m_SeenThemeSeq = initState.m_LeaderboardThemeSeq;
		m_SeenLeaderboardSeq = initState.m_LeaderboardSeq;
		m_SeenScoreboardSeq = initState.m_ScoreboardSeq;
		m_SeenPhase = initState.m_Phase;
		ApplyTheme();
		Reflow();
		return layoutRoot;
	}

	void ~DmScoreboardMenu()
	{
		RemoveInputExcludes();
		m_Pool.Clear();
		m_Own = null;
	}

	override bool UseMouse() { return true; }
	override bool UseKeyboard() { return true; }

	override void OnShow()
	{
		super.OnShow();
		AddInputExcludesIfLoaded();
		SetFocus(layoutRoot);
		RequestPage(0, false, 0);
	}

	override void OnHide()
	{
		super.OnHide();
		RemoveInputExcludes();
		m_Loading = false;
		m_FindPending = false;
		m_ApplyPendingLocal = false;
	}

	private void AddInputExcludesIfLoaded()
	{
		if (m_InputExcluded || !GetGame().GetMission()) return;
		PlayerBase inputPlayer = PlayerBase.Cast(GetGame().GetPlayer());
		if (!inputPlayer || !inputPlayer.IsPlayerLoaded()) return;
		GetGame().GetMission().AddActiveInputExcludes({"menu"});
		m_InputExcluded = true;
	}

	private void RemoveInputExcludes()
	{
		if (!m_InputExcluded) return;
		Mission inputMission;
		if (GetGame()) inputMission = GetGame().GetMission();
		if (inputMission) inputMission.RemoveActiveInputExcludes({"menu"}, true);
		m_InputExcluded = false;
	}

	void Tick()
	{
		AddInputExcludesIfLoaded();
		DmClientState tickState = DmClientState.GetInstance();
		tickState.PollLeaderboard();
		bool tickRefresh = false;
		bool tickReflow = false;
		if (tickState.m_LeaderboardSeq != m_SeenLeaderboardSeq)
		{
			m_SeenLeaderboardSeq = tickState.m_LeaderboardSeq;
			// Timeout/protocol failures also bump the sequence. Keep pending find
			// centering intact until an actual page arrives after the retry.
			if (tickState.m_LeaderboardError != "")
			{
				m_Loading = false;
				m_HasResponse = false;
				tickRefresh = true;
			}
			else if (AcceptResponse(tickState)) tickRefresh = true;
		}
		if (tickState.m_LeaderboardThemeSeq != m_SeenThemeSeq)
		{
			m_SeenThemeSeq = tickState.m_LeaderboardThemeSeq;
			m_Theme = tickState.m_LeaderboardTheme;
			ApplyTheme();
			tickReflow = true;
		}
		if (tickState.m_ScoreboardSeq != m_SeenScoreboardSeq)
		{
			m_SeenScoreboardSeq = tickState.m_ScoreboardSeq;
			tickRefresh = true;
		}
		if (tickState.m_Phase != m_SeenPhase)
		{
			m_SeenPhase = tickState.m_Phase;
			tickRefresh = true;
		}
		int tickWidth;
		int tickHeight;
		GetScreenSize(tickWidth, tickHeight);
		if (tickReflow || tickWidth != m_ScreenW || tickHeight != m_ScreenH)
		{
			Reflow();
			return;
		}
		if (tickRefresh) Refresh();
	}

	private bool AcceptResponse(DmClientState responseState)
	{
		// DmClientState request ids reject superseded pages before they reach
		// this store. Keep the tab check here; accept the server's clamped offset
		// when a population drop makes the requested page cease to exist.
		if (responseState.m_LeaderboardSession != m_Session) return false;
		m_Rows = responseState.m_LeaderboardRows;
		if (!m_Rows) m_Rows = new array<ref DmLeaderboardRow>;
		m_Self = responseState.m_LeaderboardSelf;
		m_Total = Math.Max(0, responseState.m_LeaderboardTotal);
		m_ResponseOffset = Math.Max(0, responseState.m_LeaderboardOffset);
		m_RequestedOffset = m_ResponseOffset;
		if (m_ApplyPendingLocal) m_LocalOffset = m_PendingLocalOffset;
		if (m_FindPending && m_Self)
		{
			int selfPageIndex = FindSelfInPage();
			if (selfPageIndex >= 0) m_LocalOffset = selfPageIndex - m_Visible / 2;
		}
		m_LocalOffset = ClampLocalOffset(m_LocalOffset);
		m_FindPending = false;
		m_ApplyPendingLocal = false;
		m_Loading = false;
		m_HasResponse = true;
		return true;
	}

	private void RequestPage(int globalOffset, bool findSelf, int localOffset)
	{
		m_RequestedOffset = Math.Max(0, globalOffset);
		m_PendingLocalOffset = Math.Max(0, localOffset);
		m_FindPending = findSelf;
		m_ApplyPendingLocal = true;
		m_Loading = true;
		m_HasResponse = false;
		DmClientState requestState = DmClientState.GetInstance();
		// Ignore a response accepted while this menu was closed; only a sequence
		// produced after this request may replace the loading view.
		m_SeenLeaderboardSeq = requestState.m_LeaderboardSeq;
		requestState.RequestLeaderboard(m_Session, m_RequestedOffset, findSelf);
		Refresh();
	}

	private int ClampLocalOffset(int candidateOffset)
	{
		int maximumOffset = Math.Max(0, m_Rows.Count() - m_Visible);
		return Math.Clamp(candidateOffset, 0, maximumOffset);
	}

	private int FindSelfInPage()
	{
		if (!m_Self) return -1;
		for (int selfIndex = 0; selfIndex < m_Rows.Count(); selfIndex++)
		{
			DmLeaderboardRow selfCandidate = m_Rows[selfIndex];
			if (selfCandidate && selfCandidate.PlayerKey == m_Self.PlayerKey) return selfIndex;
		}
		return -1;
	}

	private bool IsSelf(DmLeaderboardRow row)
	{
		return row && m_Self && row.PlayerKey != "" && row.PlayerKey == m_Self.PlayerKey;
	}

	private void Box(string widgetName, float x, float y, float width, float height)
	{
		Widget targetWidget = layoutRoot.FindAnyWidget(widgetName);
		if (!targetWidget) return;
		targetWidget.SetPos(x, y);
		targetWidget.SetSize(width, height);
		TextWidget buttonText = TextWidget.Cast(targetWidget.FindAnyWidget(widgetName + "Label"));
		if (buttonText)
		{
			buttonText.SetSize(width, height);
			buttonText.SetTextExactSize(18 * m_TextScale);
		}
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

	private void Reflow()
	{
		GetScreenSize(m_ScreenW, m_ScreenH);
		m_Scale = Math.Clamp(m_ScreenH / 1080.0, 0.85, 1.30);
		// Exact text is also scaled by screen height in DayZ. This compensates
		// for that second scale at the verified 720p, 1080p, and 1440p sizes.
		m_TextScale = m_Scale * 1080.0 / m_ScreenH;
		m_Width = Math.Min(1000 * m_Scale, m_ScreenW - 64);
		m_RowHeight = 38 * m_Scale;
		float availableHeight = Math.Min(m_ScreenH - 100, 920 * m_Scale);
		m_Visible = Math.Clamp(Math.Floor((availableHeight - 388 * m_Scale) / m_RowHeight + 0.01), 6, ROW_POOL_SIZE);
		m_BodyY = 244 * m_Scale;
		m_BodyHeight = m_Visible * m_RowHeight;
		float boardHeight = m_BodyY + m_BodyHeight + 144 * m_Scale;
		layoutRoot.SetPos((m_ScreenW - m_Width) / 2, (m_ScreenH - boardHeight) / 2);
		layoutRoot.SetSize(m_Width, boardHeight);
		Box("Backdrop", 0, 0, m_Width, boardHeight);
		Box("Empty", 26 * m_Scale, m_BodyY + m_BodyHeight / 2 - 24 * m_Scale, m_Width - 52 * m_Scale, 48 * m_Scale);
		Box("Accent", 0, 0, m_Width, 3 * m_Scale);
		Box("Logo", 26 * m_Scale, 23 * m_Scale, 38 * m_Scale, 38 * m_Scale);
		float brandX = 26 * m_Scale;
		if (m_LogoLoaded) brandX = 78 * m_Scale;
		Box("Brand", brandX, 20 * m_Scale, m_Width - brandX - 26 * m_Scale, 42 * m_Scale);
		Box("Subtitle", 26 * m_Scale, 65 * m_Scale, m_Width - 340 * m_Scale, 25 * m_Scale);
		Box("Title", 26 * m_Scale, 100 * m_Scale, m_Width - 52 * m_Scale, 46 * m_Scale);
		Box("Status", m_Width - 300 * m_Scale, 66 * m_Scale, 274 * m_Scale, 25 * m_Scale);
		Box("Round", 26 * m_Scale, 158 * m_Scale, 170 * m_Scale, 36 * m_Scale);
		Box("Session", 206 * m_Scale, 158 * m_Scale, 150 * m_Scale, 36 * m_Scale);
		Box("Count", m_Width - 250 * m_Scale, 158 * m_Scale, 224 * m_Scale, 36 * m_Scale);
		Box("HeaderBackground", 0, 208 * m_Scale, m_Width, 36 * m_Scale);
		Box("Header", 0, 208 * m_Scale, m_Width - 24 * m_Scale, 36 * m_Scale);
		float dataWidth = m_Width - 24 * m_Scale;
		Box("Rows", 0, m_BodyY, dataWidth, m_BodyHeight);
		for (int reflowIndex = 0; reflowIndex < m_Pool.Count(); reflowIndex++)
		{
			m_Pool[reflowIndex].Geometry(dataWidth, m_RowHeight, m_Scale, m_TextScale);
			m_Pool[reflowIndex].Root.SetPos(0, reflowIndex * m_RowHeight);
		}
		float killsX;
		float killsY;
		float deathsX;
		float deathsY;
		float streakX;
		float streakY;
		m_Pool[0].Kills.GetPos(killsX, killsY);
		m_Pool[0].Deaths.GetPos(deathsX, deathsY);
		m_Pool[0].Streak.GetPos(streakX, streakY);
		Box("H_Rank", 26 * m_Scale, 0, 64 * m_Scale, 36 * m_Scale);
		Box("H_Name", 100 * m_Scale, 0, m_Pool[0].NameWidth, 36 * m_Scale);
		Box("H_Kills", killsX, 0, 88 * m_Scale, 36 * m_Scale);
		Box("H_Deaths", deathsX, 0, 88 * m_Scale, 36 * m_Scale);
		Box("H_Streak", streakX, 0, 130 * m_Scale, 36 * m_Scale);
		Box("ScrollUp", dataWidth, m_BodyY, 24 * m_Scale, 26 * m_Scale);
		Box("ScrollTrack", dataWidth, m_BodyY + 28 * m_Scale, 24 * m_Scale, m_BodyHeight - 56 * m_Scale);
		Box("ScrollDown", dataWidth, m_BodyY + m_BodyHeight - 26 * m_Scale, 24 * m_Scale, 26 * m_Scale);
		float ownY = m_BodyY + m_BodyHeight + 8 * m_Scale;
		Box("Own", 0, ownY, dataWidth, 42 * m_Scale);
		m_Own.Geometry(dataWidth, 42 * m_Scale, m_Scale, m_TextScale);
		float controlsY = ownY + 56 * m_Scale;
		Box("Range", 26 * m_Scale, controlsY, 400 * m_Scale, 34 * m_Scale);
		Box("Find", m_Width - 358 * m_Scale, controlsY, 150 * m_Scale, 36 * m_Scale);
		Box("Close", m_Width - 198 * m_Scale, controlsY, 172 * m_Scale, 36 * m_Scale);
		Box("FooterLine", 26 * m_Scale, boardHeight - 34 * m_Scale, m_Width - 52 * m_Scale, 1);
		Box("Footer", 26 * m_Scale, boardHeight - 30 * m_Scale, m_Width - 350 * m_Scale, 24 * m_Scale);
		Box("AttributionBackground", m_Width - 322 * m_Scale, boardHeight - 31 * m_Scale, 296 * m_Scale, 26 * m_Scale);
		Box("Attribution", m_Width - 314 * m_Scale, boardHeight - 30 * m_Scale, 288 * m_Scale, 24 * m_Scale);
		SizeText("Brand", 25);
		SizeText("Subtitle", 16);
		SizeText("Title", 30);
		SizeText("Status", 16);
		SizeText("Count", 16);
		SizeText("Range", 16);
		SizeText("Footer", 15);
		SizeText("Attribution", 14);
		SizeText("Empty", 22);
		SizeText("H_Rank", 16);
		SizeText("H_Name", 16);
		SizeText("H_Kills", 16);
		SizeText("H_Deaths", 16);
		SizeText("H_Streak", 16);
		m_LocalOffset = ClampLocalOffset(m_LocalOffset);
		Refresh();
	}

	private void SizeText(string widgetName, int pixels)
	{
		TextWidget textWidget = TextWidget.Cast(layoutRoot.FindAnyWidget(widgetName));
		if (textWidget) textWidget.SetTextExactSize(pixels * m_TextScale);
	}

	private void SetLabel(string widgetName, string label)
	{
		TextWidget labelWidget = TextWidget.Cast(layoutRoot.FindAnyWidget(widgetName));
		if (labelWidget) labelWidget.SetText(label);
	}

	private void FitLabel(string widgetName, string label)
	{
		TextWidget fitWidget = TextWidget.Cast(layoutRoot.FindAnyWidget(widgetName));
		if (!fitWidget) return;
		float fitWidth;
		float fitHeight;
		fitWidget.GetSize(fitWidth, fitHeight);
		fitWidget.SetText(label);
		int measuredWidth;
		int measuredHeight;
		fitWidget.GetTextSize(measuredWidth, measuredHeight);
		int fitLength = label.Length();
		while (measuredWidth > fitWidth && fitLength > 3)
		{
			fitLength = fitLength - 1;
			fitWidget.SetText(label.Substring(0, fitLength) + "...");
			fitWidget.GetTextSize(measuredWidth, measuredHeight);
		}
	}

	private void ApplyTheme()
	{
		if (!m_Theme) m_Theme = new DmLeaderboardTheme();
		m_Theme.Validate();
		layoutRoot.FindAnyWidget("Backdrop").SetColor(m_Theme.Surface());
		layoutRoot.FindAnyWidget("Accent").SetColor(m_Theme.Accent());
		layoutRoot.FindAnyWidget("TabAccent").SetColor(m_Theme.Accent());
		SetLabel("Brand", m_Theme.BrandName);
		SetLabel("Subtitle", m_Theme.Subtitle);
		SetLabel("Footer", m_Theme.Footer);
		SetLabel("Attribution", "Powered by Sentinel Deathmatch");
		ImageWidget logoWidget = ImageWidget.Cast(layoutRoot.FindAnyWidget("Logo"));
		logoWidget.SetFlags(WidgetFlags.STRETCH | WidgetFlags.BLEND | WidgetFlags.SOURCEALPHA);
		bool loadedLogo = false;
		if (m_Theme.ShowLogo && m_Theme.LogoPath != "") loadedLogo = logoWidget.LoadImageFile(0, m_Theme.LogoPath);
		logoWidget.Show(loadedLogo);
		m_LogoLoaded = loadedLogo;
		if (loadedLogo) logoWidget.SetImage(0);
	}

	override void Refresh()
	{
		DmClientState refreshState = DmClientState.GetInstance();
		m_LocalOffset = ClampLocalOffset(m_LocalOffset);
		for (int bindIndex = 0; bindIndex < m_Pool.Count(); bindIndex++)
		{
			int rowIndex = m_LocalOffset + bindIndex;
			if (m_HasResponse && bindIndex < m_Visible && rowIndex < m_Rows.Count())
			{
				DmLeaderboardRow bindRow = m_Rows[rowIndex];
				m_Pool[bindIndex].Bind(bindRow, m_Theme, IsSelf(bindRow), ((m_ResponseOffset + rowIndex) % 2) == 1);
			}
			else m_Pool[bindIndex].Root.Show(false);
		}
		if (m_HasResponse && m_Self) m_Own.Bind(m_Self, m_Theme, true, false);
		else m_Own.Root.Show(false);
		string emptyLabel = "NO STANDINGS YET";
		if (m_Loading) emptyLabel = "LOADING STANDINGS...";
		else if (refreshState.m_LeaderboardError != "") emptyLabel = refreshState.m_LeaderboardError;
		SetLabel("Empty", emptyLabel);
		bool showEmpty = m_Loading || refreshState.m_LeaderboardError != "" || (m_HasResponse && m_Rows.Count() == 0);
		layoutRoot.FindAnyWidget("Empty").Show(showEmpty);
		layoutRoot.FindAnyWidget("Find").Enable(m_HasResponse && m_Self && !m_Loading);
		int firstVisible = 0;
		int lastVisible = 0;
		if (m_HasResponse && m_Rows.Count() > 0)
		{
			firstVisible = m_ResponseOffset + m_LocalOffset + 1;
			lastVisible = Math.Min(m_Total, firstVisible + Math.Min(m_Visible, m_Rows.Count() - m_LocalOffset) - 1);
		}
		FitLabel("Range", firstVisible.ToString() + " - " + lastVisible.ToString() + " OF " + m_Total.ToString());
		string playerSuffix = " PLAYERS";
		if (m_Total == 1) playerSuffix = " PLAYER";
		FitLabel("Count", m_Total.ToString() + playerSuffix);
		string titleText = "ROUND STANDINGS";
		string statusText = "ROUND " + refreshState.m_RoundId.ToString() + " / " + DmPhase.Name(refreshState.m_Phase);
		if (refreshState.m_Phase == DmPhase.ROUNDEND)
		{
			titleText = "ROUND COMPLETE";
			if (refreshState.m_WinnerName != "") titleText = titleText + " - " + refreshState.m_WinnerName + " WINS";
		}
		if (m_Session)
		{
			titleText = "SESSION STANDINGS";
			statusText = "SINCE SERVER RESTART";
		}
		FitLabel("Title", titleText);
		FitLabel("Status", statusText);
		FitLabel("Brand", m_Theme.BrandName);
		FitLabel("Subtitle", m_Theme.Subtitle);
		FitLabel("Footer", m_Theme.Footer);
		FitLabel("Attribution", "Powered by Sentinel Deathmatch");
		float tabX = 26 * m_Scale;
		float tabWidth = 170 * m_Scale;
		if (m_Session)
		{
			tabX = 206 * m_Scale;
			tabWidth = 150 * m_Scale;
		}
		Box("TabAccent", tabX, 194 * m_Scale, tabWidth, 3 * m_Scale);
		bool canScroll = m_HasResponse && m_Total > m_Visible;
		layoutRoot.FindAnyWidget("ScrollUp").Show(canScroll);
		layoutRoot.FindAnyWidget("ScrollDown").Show(canScroll);
		layoutRoot.FindAnyWidget("ScrollTrack").Show(canScroll);
		layoutRoot.FindAnyWidget("ScrollThumb").Show(canScroll);
		if (canScroll)
		{
			float trackHeight = m_BodyHeight - 56 * m_Scale;
			float thumbHeight = Math.Max(24 * m_Scale, trackHeight * m_Visible / Math.Max(m_Visible, m_Total));
			int absoluteOffset = m_ResponseOffset + m_LocalOffset;
			float scrollProgress = absoluteOffset / (Math.Max(1, m_Total - m_Visible) + 0.0);
			Box("ScrollThumb", m_Width - 20 * m_Scale, m_BodyY + 28 * m_Scale + scrollProgress * (trackHeight - thumbHeight), 16 * m_Scale, thumbHeight);
		}
	}

	private void ScrollRows(int delta)
	{
		if (m_Loading || !m_HasResponse) return;
		int desiredLocal = m_LocalOffset + delta;
		int localMaximum = Math.Max(0, m_Rows.Count() - m_Visible);
		if (desiredLocal < 0 && m_ResponseOffset > 0)
		{
			int previousOffset = Math.Max(0, m_ResponseOffset + desiredLocal);
			RequestPage(previousOffset, false, 0);
			return;
		}
		if (desiredLocal > localMaximum && m_ResponseOffset + m_Rows.Count() < m_Total)
		{
			int maximumGlobalOffset = Math.Max(0, m_Total - m_Visible);
			int nextOffset = Math.Clamp(m_ResponseOffset + desiredLocal, 0, maximumGlobalOffset);
			RequestPage(nextOffset, false, 0);
			return;
		}
		m_LocalOffset = Math.Clamp(desiredLocal, 0, localMaximum);
		Refresh();
	}

	private void FindMe()
	{
		if (m_Loading) return;
		RequestPage(0, true, 0);
	}

	private void SetPeriod(bool session)
	{
		if (m_Session == session && m_HasResponse) return;
		m_Session = session;
		m_LocalOffset = 0;
		m_Total = 0;
		m_Rows = new array<ref DmLeaderboardRow>;
		m_Self = null;
		RequestPage(0, false, 0);
	}

	private void JumpToTrack(Widget trackWidget, int y)
	{
		if (m_Loading || !m_HasResponse || m_Total <= m_Visible) return;
		float trackX;
		float trackY;
		trackWidget.GetScreenPos(trackX, trackY);
		float trackHeight = m_BodyHeight - 56 * m_Scale;
		float trackFraction = Math.Clamp((y - trackY) / trackHeight, 0, 1);
		int maximumGlobalOffset = Math.Max(0, m_Total - m_Visible);
		int targetGlobalOffset = trackFraction * maximumGlobalOffset;
		if (targetGlobalOffset >= m_ResponseOffset && targetGlobalOffset <= m_ResponseOffset + Math.Max(0, m_Rows.Count() - m_Visible))
		{
			m_LocalOffset = targetGlobalOffset - m_ResponseOffset;
			Refresh();
			return;
		}
		RequestPage(targetGlobalOffset, false, 0);
	}

	override bool OnMouseWheel(Widget w, int x, int y, int wheel)
	{
		if (wheel > 0) ScrollRows(-3);
		else if (wheel < 0) ScrollRows(3);
		return true;
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (button != MouseState.LEFT) return super.OnClick(w, x, y, button);
		string clickedName = w.GetName();
		if (clickedName == "Round") SetPeriod(false);
		else if (clickedName == "Session") SetPeriod(true);
		else if (clickedName == "Find") FindMe();
		else if (clickedName == "Close") Close();
		else if (clickedName == "ScrollUp") ScrollRows(-m_Visible);
		else if (clickedName == "ScrollDown") ScrollRows(m_Visible);
		else if (clickedName == "ScrollTrack") JumpToTrack(w, y);
		else return super.OnClick(w, x, y, button);
		return true;
	}
}
