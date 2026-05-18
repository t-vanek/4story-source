using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TGUILDTACTICSWANTEDTABLE
{
    public int dwID { get; set; }

    public int dwGuildID { get; set; }

    public byte bMaxLevel { get; set; }

    public byte bMinLevel { get; set; }

    public DateTime dEndTime { get; set; }

    public string szTitle { get; set; } = null!;

    public string szText { get; set; } = null!;

    public byte bDay { get; set; }

    public int dwGold { get; set; }

    public int dwSilver { get; set; }

    public int dwCooper { get; set; }

    public int dwPvPoint { get; set; }
}
