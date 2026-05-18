using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TNPCCHART
{
    public short wID { get; set; }

    public string NC_szName2 { get; set; } = null!;

    public string szName { get; set; } = null!;

    public byte bType { get; set; }

    public int dwClass { get; set; }

    public byte bCountryID { get; set; }

    public short wLocalID { get; set; }

    public byte bCondition { get; set; }

    public byte bDiscountRate { get; set; }

    public byte bAddProb { get; set; }

    public short wItemID { get; set; }

    public short wMapID { get; set; }

    public double fPosX { get; set; }

    public double fPosY { get; set; }

    public double fPosZ { get; set; }

    public string szLocal { get; set; } = null!;
}
