// Bounded wire pages, assembled once per vote window. Display labels never
// change the server's enabled-zone / valid-preset index or configuration name.
class DmVoteOptions
{
	static const int CHUNK_SIZE = 100;
	static const int LABEL_LENGTH = 128;
	int WindowId = 0;
	int ZoneTotal = 0;
	int PresetTotal = 0;
	bool Complete = false;
	ref array<string> Zones = new array<string>;
	ref array<string> Presets = new array<string>;
	private ref map<int, bool> m_Received = new map<int, bool>;
	private int m_ReceivedCount = 0;

	static string Label(string value)
	{
		return DmLeaderboardTheme.LimitString(value, LABEL_LENGTH);
	}

	static int CountAt(int total, int offset)
	{
		if (offset >= total) return 0;
		int count = total - offset;
		if (count > CHUNK_SIZE) count = CHUNK_SIZE;
		return count;
	}

	static array<string> Chunk(array<string> source, int offset)
	{
		array<string> result = new array<string>;
		int count = CountAt(source.Count(), offset);
		for (int chunkIdx = 0; chunkIdx < count; chunkIdx++) result.Insert(source[offset + chunkIdx]);
		return result;
	}

	private static bool ValidLabels(array<string> labels, int expected)
	{
		if (!labels || labels.Count() != expected) return false;
		for (int labelIdx = 0; labelIdx < labels.Count(); labelIdx++)
		{
			if (labels[labelIdx].Length() > LABEL_LENGTH) return false;
			if (labels[labelIdx] != Label(labels[labelIdx])) return false;
		}
		return true;
	}

	// True means the complete snapshot can be published atomically. Invalid,
	// duplicate and stale pages cannot reset or partially replace that snapshot.
	bool Accept(int windowId, int zoneTotal, int presetTotal, int offset, array<string> zones, array<string> presets)
	{
		if (windowId <= 0 || windowId < WindowId || zoneTotal < 0 || presetTotal < 0 || offset < 0) return false;
		if (windowId == WindowId && Complete) return false;
		int maxTotal = zoneTotal;
		if (presetTotal > maxTotal) maxTotal = presetTotal;
		int chunkNumber = offset / CHUNK_SIZE;
		if (offset != chunkNumber * CHUNK_SIZE) return false;
		if (maxTotal == 0 && offset != 0) return false;
		if (maxTotal > 0 && offset >= maxTotal) return false;
		if (!ValidLabels(zones, CountAt(zoneTotal, offset)) || !ValidLabels(presets, CountAt(presetTotal, offset))) return false;
		if (windowId == WindowId && (zoneTotal != ZoneTotal || presetTotal != PresetTotal)) return false;
		if (windowId > WindowId)
		{
			WindowId = windowId;
			ZoneTotal = zoneTotal;
			PresetTotal = presetTotal;
			Zones = new array<string>;
			Presets = new array<string>;
			Zones.Resize(zoneTotal);
			Presets.Resize(presetTotal);
			m_Received.Clear();
			m_ReceivedCount = 0;
			Complete = false;
		}
		bool received;
		if (m_Received.Find(offset, received)) return false;
		for (int zoneIdx = 0; zoneIdx < zones.Count(); zoneIdx++) Zones[offset + zoneIdx] = zones[zoneIdx];
		for (int presetIdx = 0; presetIdx < presets.Count(); presetIdx++) Presets[offset + presetIdx] = presets[presetIdx];
		m_Received.Set(offset, true);
		m_ReceivedCount = m_ReceivedCount + 1;
		int pageCount = 1;
		if (maxTotal > 0) pageCount = ((maxTotal - 1) / CHUNK_SIZE) + 1;
		Complete = m_ReceivedCount == pageCount;
		return Complete;
	}

	static void SelfTest()
	{
		array<string> source = new array<string>;
		for (int sourceIdx = 0; sourceIdx < 1000; sourceIdx++) source.Insert("Option " + sourceIdx.ToString());
		ref array<string> none = new array<string>;
		ref array<string> firstChunk = Chunk(source, 0);
		ref array<string> secondChunk = Chunk(source, 100);
		ref array<string> lastChunk = Chunk(source, 900);
		DmVoteOptions ordered = new DmVoteOptions();
		bool orderedOk = !ordered.Accept(1, 1000, 1000, 900, lastChunk, lastChunk);
		orderedOk = orderedOk && !ordered.Accept(1, 1000, 1000, 900, lastChunk, lastChunk);
		for (int pageIdx = 0; pageIdx < 9; pageIdx++)
		{
			ref array<string> pageChunk = Chunk(source, pageIdx * CHUNK_SIZE);
			bool pageComplete = ordered.Accept(1, 1000, 1000, pageIdx * CHUNK_SIZE, pageChunk, pageChunk);
			if (pageComplete != (pageIdx == 8)) orderedOk = false;
		}
		orderedOk = orderedOk && ordered.Zones.Count() == 1000 && ordered.Zones[999] == "Option 999" && ordered.Presets[100] == "Option 100";
		Print("[DM] fixture DmVoteOptions 1000 ordered assembly: expected=1 got=" + orderedOk.ToString() + " " + DmFixture.Verdict(orderedOk));

		DmVoteOptions partial = new DmVoteOptions();
		bool staleOk = !partial.Accept(2, 1000, 0, 0, firstChunk, none);
		staleOk = staleOk && partial.Accept(3, 0, 0, 0, none, none);
		staleOk = staleOk && !partial.Accept(2, 1000, 0, 100, secondChunk, none) && partial.WindowId == 3 && partial.Zones.Count() == 0;
		staleOk = staleOk && !partial.Accept(3, 0, 0, 0, none, none);
		Print("[DM] fixture DmVoteOptions newer stale duplicate: expected=1 got=" + staleOk.ToString() + " " + DmFixture.Verdict(staleOk));

		DmVoteOptions invalid = new DmVoteOptions();
		array<string> one = new array<string>;
		one.Insert("Last");
		bool invalidOk = !invalid.Accept(1, 101, 0, 1, firstChunk, none);
		invalidOk = invalidOk && !invalid.Accept(1, 101, 0, 100, firstChunk, none);
		invalidOk = invalidOk && !invalid.Accept(1, -1, 0, 0, none, none) && invalid.WindowId == 0;
		invalidOk = invalidOk && !invalid.Accept(1, 101, 0, 0, firstChunk, none);
		invalidOk = invalidOk && !invalid.Accept(1, 201, 0, 100, secondChunk, none);
		invalidOk = invalidOk && invalid.Accept(1, 101, 0, 100, one, none) && invalid.Zones[100] == "Last";
		Print("[DM] fixture DmVoteOptions bounds and final chunk: expected=1 got=" + invalidOk.ToString() + " " + DmFixture.Verdict(invalidOk));

		string longLabel = "";
		for (int longIdx = 0; longIdx < 150; longIdx++) longLabel = longLabel + "x";
		array<string> bad = new array<string>;
		bad.Insert(longLabel);
		DmVoteOptions labelProbe = new DmVoteOptions();
		bool labelOk = Label("one\ntwo\rthree\tfour") == "one two three four" && Label(longLabel).Length() == LABEL_LENGTH;
		labelOk = labelOk && !labelProbe.Accept(1, 1, 0, 0, bad, none);
		bad[0] = Label(longLabel);
		labelOk = labelOk && labelProbe.Accept(1, 1, 0, 0, bad, none);
		Print("[DM] fixture DmVoteOptions label boundary: expected=1 got=" + labelOk.ToString() + " " + DmFixture.Verdict(labelOk));
	}
}
