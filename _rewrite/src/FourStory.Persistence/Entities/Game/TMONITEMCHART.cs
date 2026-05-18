using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TMONITEMCHART
{
    public byte bChartType { get; set; }

    public short wMonID { get; set; }

    public short wItemID { get; set; }

    public short wItemIDMin { get; set; }

    public short wItemIDMax { get; set; }

    public byte bLevelMin { get; set; }

    public byte bLevelMax { get; set; }

    public byte bItemProb_N1 { get; set; }

    public byte bItemProb_N2 { get; set; }

    public byte bItemProb_N3 { get; set; }

    public byte bItemProb_N4 { get; set; }

    public byte bItemProb_M { get; set; }

    public byte bItemProb_S { get; set; }

    public byte bItemProb_R { get; set; }

    public byte bItemMagicOpt { get; set; }

    public byte bItemRareOpt { get; set; }

    public short wWeight { get; set; }
}
