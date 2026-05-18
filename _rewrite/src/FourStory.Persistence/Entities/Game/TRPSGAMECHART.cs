using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TRPSGAMECHART
{
    public byte bType { get; set; }

    public byte bWinCount { get; set; }

    public int dwRewardMoney { get; set; }

    public short wRewardItem_1 { get; set; }

    public short wRewardItem_2 { get; set; }

    public byte bItemCount_1 { get; set; }

    public byte bItemCount_2 { get; set; }

    public byte bProb_Win { get; set; }

    public byte bProb_Draw { get; set; }

    public byte bProb_Lose { get; set; }

    public short wWinKeep { get; set; }

    public short wWinPeriod { get; set; }

    public short wItemID { get; set; }
}
