using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TREGIONCHART
{
    public int dwID { get; set; }

    public string szName { get; set; } = null!;

    public short wCountryID { get; set; }

    public byte bCanfly { get; set; }

    public byte bLocal { get; set; }

    public double fDPosX { get; set; }

    public double fDPosY { get; set; }

    public double fDPosZ { get; set; }

    public double fCPosX { get; set; }

    public double fCPosY { get; set; }

    public double fCPosZ { get; set; }

    public double fBPosX { get; set; }

    public double fBPosY { get; set; }

    public double fBPosZ { get; set; }

    public byte bCanMail { get; set; }
}
