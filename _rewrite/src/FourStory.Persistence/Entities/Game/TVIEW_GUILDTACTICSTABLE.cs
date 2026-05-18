using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TVIEW_GUILDTACTICSTABLE
{
    public int dwGuildID { get; set; }

    public int dwCharID { get; set; }

    public string szNAME { get; set; } = null!;

    public byte bLevel { get; set; }

    public byte bClass { get; set; }

    public int dwRewardPoint { get; set; }

    public int dwGainPoint { get; set; }

    public byte bDay { get; set; }

    public DateTime dEndTime { get; set; }

    public long dlRewardMoney { get; set; }
}
