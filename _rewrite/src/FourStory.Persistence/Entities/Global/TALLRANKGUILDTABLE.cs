using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TALLRANKGUILDTABLE
{
    public int dwSeq { get; set; }

    public DateTime dDate { get; set; }

    public byte bRankType { get; set; }

    public byte bWorld { get; set; }

    public int dwGuildSeq { get; set; }

    public string szGuildName { get; set; } = null!;

    public int dwChief { get; set; }

    public string szChiefName { get; set; } = null!;

    public byte bLevel { get; set; }

    public int dwExp { get; set; }

    public int dwMemberCount { get; set; }

    public DateTime timeEstablish { get; set; }

    public int dwPlayTime { get; set; }

    public int? nRank { get; set; }

    public int? nRankUpDown { get; set; }
}
