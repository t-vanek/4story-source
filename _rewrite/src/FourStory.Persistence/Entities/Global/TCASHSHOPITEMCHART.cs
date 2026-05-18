using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TCASHSHOPITEMCHART
{
    public short wID { get; set; }

    public string szName { get; set; } = null!;

    public int dwMoney { get; set; }

    public short wItemID { get; set; }

    public int wInfoID { get; set; }

    public byte bLevel { get; set; }

    public byte bCount { get; set; }

    public byte bGLevel { get; set; }

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

    public byte bCanSell { get; set; }

    public byte bCategory { get; set; }

    public byte bKind { get; set; }

    public short wOrder { get; set; }

    public byte bSaleValue { get; set; }

    public int dwQuantity { get; set; }

    public short? wIconID { get; set; }

    public byte? bLimitedType { get; set; }

    public DateTime? dLimitedEnd { get; set; }

    public byte? bItemCountry { get; set; }

    public int? dwClassID { get; set; }
}
