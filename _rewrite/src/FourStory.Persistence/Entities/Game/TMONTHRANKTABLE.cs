using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TMONTHRANKTABLE
{
    public byte bMonth { get; set; }

    public byte bCountry { get; set; }

    public byte bRank { get; set; }

    public byte bMonthRank { get; set; }

    public int dwTotalRank { get; set; }

    public int dwCharID { get; set; }

    public string szName { get; set; } = null!;

    public int dwTotalPoint { get; set; }

    public int dwMonthPoint { get; set; }

    public short wMonthWin { get; set; }

    public short wMonthLose { get; set; }

    public int dwTotalWin { get; set; }

    public int dwTotalLose { get; set; }

    public byte bLevel { get; set; }

    public byte bClass { get; set; }

    public byte bRace { get; set; }

    public byte bSex { get; set; }

    public byte bHair { get; set; }

    public byte bFace { get; set; }

    public string szSay { get; set; } = null!;

    public string szGuild { get; set; } = null!;
}
