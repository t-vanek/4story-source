using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TCASHITEMINFOCHART
{
    public short wCashItemID { get; set; }

    public string szName { get; set; } = null!;

    public string szUseTime { get; set; } = null!;

    public string szExplain { get; set; } = null!;

    public byte bSellType { get; set; }
}
