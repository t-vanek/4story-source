using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TALLRANKTABLE
{
    public int dwSeq { get; set; }

    public DateTime dDate { get; set; }

    public byte bWorld { get; set; }

    public byte bRankType { get; set; }

    public int? dwUserID { get; set; }

    public int dwCharID { get; set; }

    public int nRank { get; set; }

    public int? nRankUpDown { get; set; }

    public string szCharName { get; set; } = null!;

    public byte bCountry { get; set; }

    public byte bClass { get; set; }

    public byte bRace { get; set; }

    public byte bSex { get; set; }

    public int bLevel { get; set; }
}
