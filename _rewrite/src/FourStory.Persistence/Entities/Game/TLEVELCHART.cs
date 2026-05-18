using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TLEVELCHART
{
    public byte bLevel { get; set; }

    public int dwEXP { get; set; }

    public int dwHP { get; set; }

    public int dwMP { get; set; }

    public byte bSkillPoint { get; set; }

    public int dwMoney { get; set; }

    public int dwScore { get; set; }

    public int dwRegCost { get; set; }

    public int dwSearchCost { get; set; }

    public int dwGambleCost { get; set; }

    public int dwRepCost { get; set; }

    public int dwRepairCost { get; set; }

    public int dwRefineCost { get; set; }

    public short wPvPoint { get; set; }

    public int dwPvPMoney { get; set; }

    public int dwPvPExp { get; set; }
}
