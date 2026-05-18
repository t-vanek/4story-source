using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TQUESTITEMCHART
{
    public int dwID { get; set; }

    public short wItemID { get; set; }

    public byte bLevel { get; set; }

    public byte bGLevel { get; set; }

    public byte bDropLevel { get; set; }

    public int dwDuraMax { get; set; }

    public int dwDuraCur { get; set; }

    public byte bRefineCur { get; set; }

    public short wUseTime { get; set; }

    public byte bGradeEffect { get; set; }

    public byte bMagic1 { get; set; }

    public byte bMagic2 { get; set; }

    public byte bMagic3 { get; set; }

    public byte bMagic4 { get; set; }

    public byte bMagic5 { get; set; }

    public byte bMagic6 { get; set; }

    public short wValue1 { get; set; }

    public short wValue2 { get; set; }

    public short wValue3 { get; set; }

    public short wValue4 { get; set; }

    public short wValue5 { get; set; }

    public short wValue6 { get; set; }

    public int dwTime1 { get; set; }

    public int dwTime2 { get; set; }

    public int dwTime3 { get; set; }

    public int dwTime4 { get; set; }

    public int dwTime5 { get; set; }

    public int dwTime6 { get; set; }

    public int dwMoney { get; set; }

    public byte bGem { get; set; }
}
