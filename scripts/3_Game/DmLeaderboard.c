// Shared leaderboard DTOs and bounded wire encoding. PlayerKey is a
// server-issued session alias; provider account ids never cross this API.
class DmLeaderboardRow
{
	int Rank = 0;
	int Kills = 0;
	int Deaths = 0;
	int BestStreak = 0;
	string PlayerKey = "";
	string Name = "";
}

class DmLeaderboardPage
{
	int Revision = 0;
	int Total = 0;
	int Offset = 0;
	bool Session = false;
	ref array<ref DmLeaderboardRow> Rows = new array<ref DmLeaderboardRow>;
	ref DmLeaderboardRow SelfRow;
}

class DmLeaderboard
{
	// Leaderboard RPCs require matching client/server protocol versions; there is no legacy
	// fallback because old clients do not understand the request-driven IDs.
	static const int PROTOCOL_VERSION = 2;
	static const int MAX_ROWS = 100;
	static const int MAX_OFFSET = 1000000;
	static const int MAX_NAME_LENGTH = 128;
	static const int MAX_ENCODED_ROW_LENGTH = 256;
	static const float SERVER_RATE_SECONDS = 0.25;
	static const float CLIENT_SEND_SECONDS = 0.30;
	static const float CLIENT_POLL_SECONDS = 1.0;
	static const float CLIENT_TIMEOUT_SECONDS = 3.0;

	static int ClampOffset(int requestedOffset, int total)
	{
		if (total <= 0) return 0;
		int boundedOffset = requestedOffset;
		if (boundedOffset < 0) boundedOffset = 0;
		if (boundedOffset > DmLeaderboard.MAX_OFFSET) boundedOffset = DmLeaderboard.MAX_OFFSET;
		int lastPageOffset = ((total - 1) / DmLeaderboard.MAX_ROWS) * DmLeaderboard.MAX_ROWS;
		if (boundedOffset > lastPageOffset) boundedOffset = lastPageOffset;
		return boundedOffset;
	}

	static int OffsetForIndex(int rowIndex, int total)
	{
		if (rowIndex < 0) return 0;
		return DmLeaderboard.ClampOffset((rowIndex / DmLeaderboard.MAX_ROWS) * DmLeaderboard.MAX_ROWS, total);
	}

	static string CleanField(string value)
	{
		string cleanValue = value;
		cleanValue.Replace("\t", " ");
		cleanValue.Replace("\r", " ");
		cleanValue.Replace("\n", " ");
		if (cleanValue.Length() > DmLeaderboard.MAX_NAME_LENGTH) cleanValue = cleanValue.Substring(0, DmLeaderboard.MAX_NAME_LENGTH);
		return cleanValue;
	}

	static string EncodeRow(DmLeaderboardRow row)
	{
		if (!row) return "";
		string encodedName = DmLeaderboard.CleanField(row.Name);
		if (encodedName == "") encodedName = "Unknown";
		return row.Rank.ToString() + "\t" + DmLeaderboard.CleanField(row.PlayerKey) + "\t" + encodedName + "\t" + row.Kills.ToString() + "\t" + row.Deaths.ToString() + "\t" + row.BestStreak.ToString();
	}

	static DmLeaderboardRow DecodeRow(string encodedRow)
	{
		if (encodedRow == "") return null;
		array<string> decodedFields = new array<string>;
		encodedRow.Split("\t", decodedFields);
		if (decodedFields.Count() != 6) return null;
		DmLeaderboardRow decodedRow = new DmLeaderboardRow();
		decodedRow.Rank = decodedFields[0].ToInt();
		decodedRow.PlayerKey = decodedFields[1];
		decodedRow.Name = decodedFields[2];
		decodedRow.Kills = decodedFields[3].ToInt();
		decodedRow.Deaths = decodedFields[4].ToInt();
		decodedRow.BestStreak = decodedFields[5].ToInt();
		if (decodedRow.Rank <= 0 || decodedRow.PlayerKey == "") return null;
		if (decodedRow.Name == "") decodedRow.Name = "Unknown";
		return decodedRow;
	}

	static void EncodeRowList(array<ref DmLeaderboardRow> rows, array<string> outRows)
	{
		outRows.Clear();
		if (!rows) return;
		int encodeCount = rows.Count();
		if (encodeCount > DmLeaderboard.MAX_ROWS) encodeCount = DmLeaderboard.MAX_ROWS;
		for (int encodeIdx = 0; encodeIdx < encodeCount; encodeIdx++)
		{
			outRows.Insert(DmLeaderboard.EncodeRow(rows[encodeIdx]));
		}
	}

	static bool DecodeRowList(array<string> encodedRows, array<ref DmLeaderboardRow> outRows)
	{
		outRows.Clear();
		if (!encodedRows) return false;
		int decodeCount = encodedRows.Count();
		if (decodeCount > DmLeaderboard.MAX_ROWS) return false;
		for (int decodeIdx = 0; decodeIdx < decodeCount; decodeIdx++)
		{
			if (encodedRows[decodeIdx].Length() > DmLeaderboard.MAX_ENCODED_ROW_LENGTH)
			{
				outRows.Clear();
				return false;
			}
			DmLeaderboardRow parsedRow = DmLeaderboard.DecodeRow(encodedRows[decodeIdx]);
			if (!parsedRow)
			{
				outRows.Clear();
				return false;
			}
			outRows.Insert(parsedRow);
		}
		return true;
	}

	static void SelfTest()
	{
		int boundsOk = 1;
		if (DmLeaderboard.ClampOffset(-7, 205) != 0) boundsOk = 0;
		if (DmLeaderboard.ClampOffset(9999999, 205) != 200) boundsOk = 0;
		if (DmLeaderboard.ClampOffset(100, 60) != 0) boundsOk = 0;
		if (DmLeaderboard.ClampOffset(50, 205) != 50) boundsOk = 0;
		if (DmLeaderboard.ClampOffset(50, 0) != 0) boundsOk = 0;
		if (DmLeaderboard.OffsetForIndex(204, 205) != 200) boundsOk = 0;
		Print("[DM] fixture DmLeaderboard invalid bounds: expected=1 got=" + boundsOk.ToString() + " " + DmFixture.Verdict(boundsOk == 1));

		DmLeaderboardRow sourceRow = new DmLeaderboardRow();
		sourceRow.Rank = 7;
		sourceRow.PlayerKey = "P17";
		sourceRow.Name = "Alice\tExample";
		sourceRow.Kills = -2;
		sourceRow.Deaths = 9;
		sourceRow.BestStreak = 3;
		DmLeaderboardRow roundTripRow = DmLeaderboard.DecodeRow(DmLeaderboard.EncodeRow(sourceRow));
		int codecOk = 1;
		if (!roundTripRow) codecOk = 0;
		if (roundTripRow && roundTripRow.Rank != 7) codecOk = 0;
		if (roundTripRow && roundTripRow.PlayerKey != "P17") codecOk = 0;
		if (roundTripRow && roundTripRow.Name != "Alice Example") codecOk = 0;
		if (roundTripRow && roundTripRow.Kills != -2) codecOk = 0;
		sourceRow.Name = "";
		DmLeaderboardRow emptyNameRow = DmLeaderboard.DecodeRow(DmLeaderboard.EncodeRow(sourceRow));
		if (!emptyNameRow || emptyNameRow.Name != "Unknown") codecOk = 0;
		if (DmLeaderboard.DecodeRow("0\tP1\tBad\t0\t0\t0")) codecOk = 0;
		if (DmLeaderboard.DecodeRow("1\t\tBad\t0\t0\t0")) codecOk = 0;
		array<ref DmLeaderboardRow> sourceRows = new array<ref DmLeaderboardRow>;
		sourceRows.Insert(sourceRow);
		array<string> encodedRowList = new array<string>;
		DmLeaderboard.EncodeRowList(sourceRows, encodedRowList);
		array<ref DmLeaderboardRow> decodedRowList = new array<ref DmLeaderboardRow>;
		if (!DmLeaderboard.DecodeRowList(encodedRowList, decodedRowList)) codecOk = 0;
		if (decodedRowList.Count() != 1) codecOk = 0;
		encodedRowList.Insert("1\tP2\t" + DmLeaderboard.CleanField("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX") + "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\t0\t0\t0");
		if (DmLeaderboard.DecodeRowList(encodedRowList, decodedRowList)) codecOk = 0;
		Print("[DM] fixture DmLeaderboard row codec: expected=1 got=" + codecOk.ToString() + " " + DmFixture.Verdict(codecOk == 1));
	}
}
