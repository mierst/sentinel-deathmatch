// Per-round scoring: kills, deaths, streaks.
class DmScoreEntry
{
	string PlayerName = "";
	string PlayerKey = "";
	int Kills = 0;
	int Deaths = 0;
	int Streak = 0;
	int BestStreak = 0;
}

class DmScoreService
{
	private static ref DmScoreService s_Instance;

	// Round scores reset every round; session scores accumulate until restart.
	private ref map<string, ref DmScoreEntry> m_Scores = new map<string, ref DmScoreEntry>;
	private ref map<string, ref DmScoreEntry> m_SessionScores = new map<string, ref DmScoreEntry>;
	private ref map<string, string> m_PublicKeys = new map<string, string>;
	private int m_NextPublicKey = 1;

	// Mutations dirty snapshots. The first request rebuilds once in O(n log n);
	// other viewers read the same cache in O(page).
	private int m_RoundRevision = 0;
	private int m_SessionRevision = 0;
	private int m_RoundSnapshotRevision = -1;
	private int m_SessionSnapshotRevision = -1;
	private ref array<string> m_RoundSortedIds = new array<string>;
	private ref array<string> m_SessionSortedIds = new array<string>;
	private ref map<string, int> m_RoundSnapshotIndices = new map<string, int>;
	private ref map<string, int> m_SessionSnapshotIndices = new map<string, int>;
	private ref array<ref DmLeaderboardRow> m_RoundSnapshot = new array<ref DmLeaderboardRow>;
	private ref array<ref DmLeaderboardRow> m_SessionSnapshot = new array<ref DmLeaderboardRow>;

	static DmScoreService GetInstance()
	{
		if (!s_Instance) s_Instance = new DmScoreService();
		return s_Instance;
	}

	void Reset()
	{
		m_Scores.Clear();
		m_RoundRevision = m_RoundRevision + 1;
	}

	private string GetOrCreatePublicKey(string playerId)
	{
		string publicKey;
		if (m_PublicKeys.Find(playerId, publicKey)) return publicKey;
		publicKey = "P" + m_NextPublicKey.ToString();
		m_NextPublicKey = m_NextPublicKey + 1;
		m_PublicKeys.Set(playerId, publicKey);
		return publicKey;
	}

	private DmScoreEntry GetOrCreateIn(map<string, ref DmScoreEntry> scoreMap, string playerId, string playerName, bool session)
	{
		DmScoreEntry entry = scoreMap.Get(playerId);
		if (!entry)
		{
			entry = new DmScoreEntry();
			entry.PlayerName = playerName;
			entry.PlayerKey = GetOrCreatePublicKey(playerId);
			scoreMap.Set(playerId, entry);
			if (session) m_SessionRevision = m_SessionRevision + 1;
			else m_RoundRevision = m_RoundRevision + 1;
		}
		return entry;
	}

	DmScoreEntry GetOrCreate(string playerId, string playerName)
	{
		return GetOrCreateIn(m_Scores, playerId, playerName, false);
	}

	bool EnsurePlayers(array<Man> players)
	{
		bool changed = false;
		for (int playerIdx = 0; playerIdx < players.Count(); playerIdx++)
		{
			Man man = players[playerIdx];
			if (!man) continue;
			PlayerIdentity ident = man.GetIdentity();
			if (!ident) continue;
			string playerId = ident.GetPlainId();
			if (!m_Scores.Contains(playerId))
			{
				GetOrCreateIn(m_Scores, playerId, ident.GetName(), false);
				changed = true;
			}
			if (!m_SessionScores.Contains(playerId))
			{
				GetOrCreateIn(m_SessionScores, playerId, ident.GetName(), true);
				changed = true;
			}
		}
		return changed;
	}

	void RegisterPenalty(string playerId, string playerName)
	{
		DmScoreEntry roundEntry = GetOrCreateIn(m_Scores, playerId, playerName, false);
		roundEntry.Kills = roundEntry.Kills - 1;
		m_RoundRevision = m_RoundRevision + 1;
		DmScoreEntry sessionEntry = GetOrCreateIn(m_SessionScores, playerId, playerName, true);
		sessionEntry.Kills = sessionEntry.Kills - 1;
		m_SessionRevision = m_SessionRevision + 1;
	}

	int RegisterKill(string killerId, string killerName, string victimId, string victimName)
	{
		DmScoreEntry victimEntry = GetOrCreateIn(m_Scores, victimId, victimName, false);
		victimEntry.Deaths = victimEntry.Deaths + 1;
		victimEntry.Streak = 0;
		DmScoreEntry victimSession = GetOrCreateIn(m_SessionScores, victimId, victimName, true);
		victimSession.Deaths = victimSession.Deaths + 1;
		victimSession.Streak = 0;
		m_RoundRevision = m_RoundRevision + 1;
		m_SessionRevision = m_SessionRevision + 1;
		if (killerId == "" || killerId == victimId) return 0;

		DmScoreEntry killerSession = GetOrCreateIn(m_SessionScores, killerId, killerName, true);
		killerSession.Kills = killerSession.Kills + 1;
		killerSession.Streak = killerSession.Streak + 1;
		if (killerSession.Streak > killerSession.BestStreak) killerSession.BestStreak = killerSession.Streak;
		DmScoreEntry killerEntry = GetOrCreateIn(m_Scores, killerId, killerName, false);
		killerEntry.Kills = killerEntry.Kills + 1;
		killerEntry.Streak = killerEntry.Streak + 1;
		if (killerEntry.Streak > killerEntry.BestStreak) killerEntry.BestStreak = killerEntry.Streak;
		m_RoundRevision = m_RoundRevision + 1;
		m_SessionRevision = m_SessionRevision + 1;
		return killerEntry.Streak;
	}

	int LeaderScore()
	{
		int best = 0;
		for (int scanIdx = 0; scanIdx < m_Scores.Count(); scanIdx++)
		{
			DmScoreEntry entry = m_Scores.GetElement(scanIdx);
			if (entry.Kills > best) best = entry.Kills;
		}
		return best;
	}

	string LeaderName()
	{
		DmScoreEntry leaderEntry = m_Scores.Get(LeaderId());
		if (!leaderEntry) return "";
		return leaderEntry.PlayerName;
	}

	// Equal scores compare by current position. This reproduces the old
	// selection sort's first-maximum and swap behavior exactly.
	private static bool HeapBefore(string leftId, string rightId, map<string, ref DmScoreEntry> scoreMap, map<string, int> positions)
	{
		int leftKills = scoreMap.Get(leftId).Kills;
		int rightKills = scoreMap.Get(rightId).Kills;
		if (leftKills != rightKills) return leftKills > rightKills;
		return positions.Get(leftId) < positions.Get(rightId);
	}

	private static void HeapSwap(array<string> heap, int leftIdx, int rightIdx, map<string, int> heapIndices)
	{
		string leftValue = heap[leftIdx];
		string rightValue = heap[rightIdx];
		heap[leftIdx] = rightValue;
		heap[rightIdx] = leftValue;
		heapIndices.Set(rightValue, leftIdx);
		heapIndices.Set(leftValue, rightIdx);
	}

	private static void HeapSiftDown(array<string> heap, int startIdx, map<string, ref DmScoreEntry> scoreMap, map<string, int> positions, map<string, int> heapIndices)
	{
		int siftIdx = startIdx;
		while (true)
		{
			int leftChildIdx = siftIdx * 2 + 1;
			if (leftChildIdx >= heap.Count()) return;
			int bestChildIdx = leftChildIdx;
			int rightChildIdx = leftChildIdx + 1;
			if (rightChildIdx < heap.Count() && DmScoreService.HeapBefore(heap[rightChildIdx], heap[leftChildIdx], scoreMap, positions)) bestChildIdx = rightChildIdx;
			if (!DmScoreService.HeapBefore(heap[bestChildIdx], heap[siftIdx], scoreMap, positions)) return;
			DmScoreService.HeapSwap(heap, siftIdx, bestChildIdx, heapIndices);
			siftIdx = bestChildIdx;
		}
	}

	static void BuildSortedIdsFor(map<string, ref DmScoreEntry> scoreMap, array<string> outIds)
	{
		outIds.Clear();
		array<string> sortHeap = new array<string>;
		map<string, int> currentPositions = new map<string, int>;
		map<string, int> heapIndices = new map<string, int>;
		for (int collectIdx = 0; collectIdx < scoreMap.Count(); collectIdx++)
		{
			string collectId = scoreMap.GetKey(collectIdx);
			outIds.Insert(collectId);
			sortHeap.Insert(collectId);
			currentPositions.Set(collectId, collectIdx);
			heapIndices.Set(collectId, collectIdx);
		}
		for (int heapifyIdx = (sortHeap.Count() / 2) - 1; heapifyIdx >= 0; heapifyIdx--)
		{
			DmScoreService.HeapSiftDown(sortHeap, heapifyIdx, scoreMap, currentPositions, heapIndices);
		}
		for (int sortedIdx = 0; sortedIdx < outIds.Count(); sortedIdx++)
		{
			string chosenId = sortHeap[0];
			int chosenPosition = currentPositions.Get(chosenId);
			string displacedId = outIds[sortedIdx];
			int heapLastIdx = sortHeap.Count() - 1;
			if (heapLastIdx > 0)
			{
				string heapLastId = sortHeap[heapLastIdx];
				sortHeap[0] = heapLastId;
				heapIndices.Set(heapLastId, 0);
			}
			sortHeap.Remove(heapLastIdx);
			heapIndices.Remove(chosenId);
			if (sortHeap.Count() > 0) DmScoreService.HeapSiftDown(sortHeap, 0, scoreMap, currentPositions, heapIndices);
			if (chosenPosition != sortedIdx)
			{
				outIds[sortedIdx] = chosenId;
				outIds[chosenPosition] = displacedId;
				currentPositions.Set(chosenId, sortedIdx);
				currentPositions.Set(displacedId, chosenPosition);
				int displacedHeapIdx = heapIndices.Get(displacedId);
				DmScoreService.HeapSiftDown(sortHeap, displacedHeapIdx, scoreMap, currentPositions, heapIndices);
			}
		}
	}

	private void RebuildSnapshot(map<string, ref DmScoreEntry> scoreMap, array<string> sortedIds, map<string, int> snapshotIndices, array<ref DmLeaderboardRow> rows)
	{
		DmScoreService.BuildSortedIdsFor(scoreMap, sortedIds);
		snapshotIndices.Clear();
		rows.Clear();
		for (int snapshotIdx = 0; snapshotIdx < sortedIds.Count(); snapshotIdx++)
		{
			snapshotIndices.Set(sortedIds[snapshotIdx], snapshotIdx);
			DmScoreEntry snapshotEntry = scoreMap.Get(sortedIds[snapshotIdx]);
			DmLeaderboardRow snapshotRow = new DmLeaderboardRow();
			snapshotRow.Rank = snapshotIdx + 1;
			snapshotRow.PlayerKey = snapshotEntry.PlayerKey;
			snapshotRow.Name = snapshotEntry.PlayerName;
			snapshotRow.Kills = snapshotEntry.Kills;
			snapshotRow.Deaths = snapshotEntry.Deaths;
			snapshotRow.BestStreak = snapshotEntry.BestStreak;
			rows.Insert(snapshotRow);
		}
	}

	private void EnsureSnapshot(bool session)
	{
		if (session)
		{
			if (m_SessionSnapshotRevision == m_SessionRevision) return;
			RebuildSnapshot(m_SessionScores, m_SessionSortedIds, m_SessionSnapshotIndices, m_SessionSnapshot);
			m_SessionSnapshotRevision = m_SessionRevision;
			return;
		}
		if (m_RoundSnapshotRevision == m_RoundRevision) return;
		RebuildSnapshot(m_Scores, m_RoundSortedIds, m_RoundSnapshotIndices, m_RoundSnapshot);
		m_RoundSnapshotRevision = m_RoundRevision;
	}

	DmLeaderboardPage BuildLeaderboardPage(bool session, string selfId, int requestedOffset, bool findSelf)
	{
		EnsureSnapshot(session);
		map<string, int> snapshotIndices = m_RoundSnapshotIndices;
		array<ref DmLeaderboardRow> snapshotRows = m_RoundSnapshot;
		int pageRevision = m_RoundRevision;
		if (session)
		{
			snapshotIndices = m_SessionSnapshotIndices;
			snapshotRows = m_SessionSnapshot;
			pageRevision = m_SessionRevision;
		}
		DmLeaderboardPage page = new DmLeaderboardPage();
		page.Revision = pageRevision;
		page.Total = snapshotRows.Count();
		page.Session = session;
		int selfIndex = -1;
		snapshotIndices.Find(selfId, selfIndex);
		page.Offset = DmLeaderboard.ClampOffset(requestedOffset, page.Total);
		if (findSelf && selfIndex >= 0) page.Offset = DmLeaderboard.OffsetForIndex(selfIndex, page.Total);
		int pageEnd = page.Offset + DmLeaderboard.MAX_ROWS;
		if (pageEnd > page.Total) pageEnd = page.Total;
		for (int pageIdx = page.Offset; pageIdx < pageEnd; pageIdx++) page.Rows.Insert(snapshotRows[pageIdx]);
		if (selfIndex >= 0) page.SelfRow = snapshotRows[selfIndex];
		return page;
	}

	private string BuildLegacyRowsBlob(bool session)
	{
		EnsureSnapshot(session);
		array<ref DmLeaderboardRow> legacyRows = m_RoundSnapshot;
		if (session) legacyRows = m_SessionSnapshot;
		string legacyBlob = "";
		for (int legacyIdx = 0; legacyIdx < legacyRows.Count(); legacyIdx++)
		{
			DmLeaderboardRow legacyRow = legacyRows[legacyIdx];
			if (legacyIdx > 0) legacyBlob = legacyBlob + "\n";
			legacyBlob = legacyBlob + legacyRow.Name + "\t" + legacyRow.Kills.ToString() + "\t" + legacyRow.Deaths.ToString() + "\t" + legacyRow.BestStreak.ToString();
		}
		return legacyBlob;
	}

	string BuildRowsBlob() { return BuildLegacyRowsBlob(false); }
	string BuildSessionRowsBlob() { return BuildLegacyRowsBlob(true); }

	string LeaderId()
	{
		int best = -1;
		string bestId = "";
		for (int leaderIdx = 0; leaderIdx < m_Scores.Count(); leaderIdx++)
		{
			DmScoreEntry leaderCandidate = m_Scores.GetElement(leaderIdx);
			if (leaderCandidate.Kills > best)
			{
				best = leaderCandidate.Kills;
				bestId = m_Scores.GetKey(leaderIdx);
			}
		}
		return bestId;
	}

	void PrintSummary()
	{
		Print("[DM] round summary (" + m_Scores.Count().ToString() + " players):");
		for (int summaryIdx = 0; summaryIdx < m_Scores.Count(); summaryIdx++)
		{
			DmScoreEntry summaryEntry = m_Scores.GetElement(summaryIdx);
			Print("[DM]   " + summaryEntry.PlayerName + " K:" + summaryEntry.Kills.ToString() + " D:" + summaryEntry.Deaths.ToString() + " best streak:" + summaryEntry.BestStreak.ToString());
		}
	}

	private static void BuildLegacySortedIdsFor(map<string, ref DmScoreEntry> scoreMap, array<string> outIds)
	{
		outIds.Clear();
		for (int legacyCollectIdx = 0; legacyCollectIdx < scoreMap.Count(); legacyCollectIdx++) outIds.Insert(scoreMap.GetKey(legacyCollectIdx));
		for (int legacySortIdx = 0; legacySortIdx < outIds.Count(); legacySortIdx++)
		{
			int legacyMaxAt = legacySortIdx;
			for (int legacyProbeIdx = legacySortIdx + 1; legacyProbeIdx < outIds.Count(); legacyProbeIdx++)
			{
				if (scoreMap.Get(outIds[legacyProbeIdx]).Kills > scoreMap.Get(outIds[legacyMaxAt]).Kills) legacyMaxAt = legacyProbeIdx;
			}
			if (legacyMaxAt != legacySortIdx)
			{
				string legacySwapId = outIds[legacySortIdx];
				outIds[legacySortIdx] = outIds[legacyMaxAt];
				outIds[legacyMaxAt] = legacySwapId;
			}
		}
	}

	static void SelfTest()
	{
		DmScoreService probe = new DmScoreService();
		int streakOne = probe.RegisterKill("k1", "Killer", "v1", "Victim");
		int streakTwo = probe.RegisterKill("k1", "Killer", "v2", "Victim2");
		int killOk = 1;
		if (streakOne != 1 || streakTwo != 2) killOk = 0;
		if (probe.LeaderScore() != 2 || probe.LeaderId() != "k1") killOk = 0;
		Print("[DM] fixture DmScoreService kill+streak: expected=1 got=" + killOk.ToString() + " " + DmFixture.Verdict(killOk == 1));

		int suicideStreak = probe.RegisterKill("v2", "Victim2", "v2", "Victim2");
		int suicideOk = 1;
		DmScoreEntry v2Entry = probe.GetOrCreate("v2", "Victim2");
		if (suicideStreak != 0 || v2Entry.Kills != 0 || v2Entry.Deaths != 2) suicideOk = 0;
		Print("[DM] fixture DmScoreService suicide no credit: expected=1 got=" + suicideOk.ToString() + " " + DmFixture.Verdict(suicideOk == 1));

		probe.RegisterKill("k2", "K2", "k1", "Killer");
		DmScoreEntry k1Entry = probe.GetOrCreate("k1", "Killer");
		int streakResetOk = 1;
		if (k1Entry.Streak != 0 || k1Entry.BestStreak != 2) streakResetOk = 0;
		probe.Reset();
		if (probe.LeaderScore() != 0) streakResetOk = 0;
		Print("[DM] fixture DmScoreService streak reset: expected=1 got=" + streakResetOk.ToString() + " " + DmFixture.Verdict(streakResetOk == 1));

		int sessionOk = 1;
		if (probe.BuildRowsBlob() != "") sessionOk = 0;
		string sessionBlob = probe.BuildSessionRowsBlob();
		if (sessionBlob == "" || sessionBlob.IndexOf("Killer\t2") < 0) sessionOk = 0;
		Print("[DM] fixture DmScoreService session survives reset: expected=1 got=" + sessionOk.ToString() + " " + DmFixture.Verdict(sessionOk == 1));

		DmScoreService penaltyProbe = new DmScoreService();
		penaltyProbe.RegisterKill("", "", "p1", "Loner");
		penaltyProbe.RegisterPenalty("p1", "Loner");
		DmScoreEntry p1Entry = penaltyProbe.GetOrCreate("p1", "Loner");
		int penaltyOk = 1;
		if (p1Entry.Kills != -1 || p1Entry.Deaths != 1) penaltyOk = 0;
		if (penaltyProbe.BuildSessionRowsBlob().IndexOf("Loner\t-1") < 0) penaltyOk = 0;
		Print("[DM] fixture DmScoreService penalty negative: expected=1 got=" + penaltyOk.ToString() + " " + DmFixture.Verdict(penaltyOk == 1));

		DmScoreService duplicateProbe = new DmScoreService();
		DmScoreEntry duplicateFirst = duplicateProbe.GetOrCreate("76561198000000000", "First");
		DmScoreEntry duplicateSecond = duplicateProbe.GetOrCreate("76561198000000000", "Second");
		int duplicateOk = 1;
		if (duplicateFirst != duplicateSecond) duplicateOk = 0;
		if (duplicateFirst.PlayerKey == "76561198000000000") duplicateOk = 0;
		if (duplicateProbe.BuildLeaderboardPage(false, "76561198000000000", 0, false).Total != 1) duplicateOk = 0;
		Print("[DM] fixture DmScoreService duplicate identity: expected=1 got=" + duplicateOk.ToString() + " " + DmFixture.Verdict(duplicateOk == 1));

		map<string, ref DmScoreEntry> tieScores = new map<string, ref DmScoreEntry>;
		array<int> tieKills = new array<int>;
		tieKills.Insert(1);
		tieKills.Insert(3);
		tieKills.Insert(1);
		tieKills.Insert(3);
		tieKills.Insert(-2);
		tieKills.Insert(1);
		tieKills.Insert(3);
		tieKills.Insert(-2);
		tieKills.Insert(1);
		for (int tieSeedIdx = 0; tieSeedIdx < tieKills.Count(); tieSeedIdx++)
		{
			DmScoreEntry tieEntry = new DmScoreEntry();
			tieEntry.Kills = tieKills[tieSeedIdx];
			tieScores.Set("t" + tieSeedIdx.ToString(), tieEntry);
		}
		array<string> legacyOrder = new array<string>;
		array<string> heapOrder = new array<string>;
		DmScoreService.BuildLegacySortedIdsFor(tieScores, legacyOrder);
		DmScoreService.BuildSortedIdsFor(tieScores, heapOrder);
		int tiesOk = 1;
		if (legacyOrder.Count() != heapOrder.Count()) tiesOk = 0;
		for (int tieCompareIdx = 0; tieCompareIdx < legacyOrder.Count(); tieCompareIdx++)
		{
			if (legacyOrder[tieCompareIdx] != heapOrder[tieCompareIdx]) tiesOk = 0;
		}
		Print("[DM] fixture DmScoreService exact legacy ties: expected=1 got=" + tiesOk.ToString() + " " + DmFixture.Verdict(tiesOk == 1));

		DmScoreService pageProbe = new DmScoreService();
		for (int pageSeedIdx = 0; pageSeedIdx < 105; pageSeedIdx++)
		{
			DmScoreEntry pageSeedEntry = pageProbe.GetOrCreate("page-id-" + pageSeedIdx.ToString(), "Player " + pageSeedIdx.ToString());
			pageSeedEntry.Kills = pageSeedIdx % 11;
		}
		DmLeaderboardPage firstPage = pageProbe.BuildLeaderboardPage(false, "page-id-104", 0, false);
		DmLeaderboardPage clampedPage = pageProbe.BuildLeaderboardPage(false, "page-id-104", 999999, false);
		DmLeaderboardPage selfPage = pageProbe.BuildLeaderboardPage(false, "page-id-104", 0, true);
		int pagesOk = 1;
		if (firstPage.Total != 105 || firstPage.Rows.Count() != 100) pagesOk = 0;
		if (clampedPage.Offset != 100 || clampedPage.Rows.Count() != 5) pagesOk = 0;
		if (!selfPage.SelfRow) pagesOk = 0;
		if (selfPage.SelfRow && selfPage.SelfRow.PlayerKey == "page-id-104") pagesOk = 0;
		if (selfPage.Offset != DmLeaderboard.OffsetForIndex(selfPage.SelfRow.Rank - 1, 105)) pagesOk = 0;
		Print("[DM] fixture DmScoreService bounded pages: expected=1 got=" + pagesOk.ToString() + " " + DmFixture.Verdict(pagesOk == 1));
	}
}
