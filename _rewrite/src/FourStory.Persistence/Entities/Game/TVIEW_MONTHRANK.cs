using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TVIEW_MONTHRANK
{
    public int dwCharID { get; set; }

    public string szName { get; set; } = null!;

    public byte bCountry { get; set; }

    public int dwTotalPoint { get; set; }

    public int dwPoint { get; set; }

    public short wWin { get; set; }

    public short wLose { get; set; }

    public int? dwTotalWin { get; set; }

    public int? dwTotalLose { get; set; }

    public byte bLevel { get; set; }

    public byte bClass { get; set; }

    public byte bRace { get; set; }

    public byte bSex { get; set; }

    public byte bHair { get; set; }

    public byte bFace { get; set; }

    public string szSay { get; set; } = null!;

    public string? szGuild { get; set; }
}
