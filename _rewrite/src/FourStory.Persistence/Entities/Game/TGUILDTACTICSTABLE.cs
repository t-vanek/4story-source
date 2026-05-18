using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TGUILDTACTICSTABLE
{
    public int dwCharID { get; set; }

    public int dwGuildID { get; set; }

    public int dwRewardPoint { get; set; }

    public int dwGainPoint { get; set; }

    public byte bDay { get; set; }

    public DateTime dEndTime { get; set; }

    public long dlRewardMoney { get; set; }
}
