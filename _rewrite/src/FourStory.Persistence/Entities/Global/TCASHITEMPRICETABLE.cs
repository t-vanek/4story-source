using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TCASHITEMPRICETABLE
{
    public byte bPackage { get; set; }

    public short wItemID { get; set; }

    public byte bAmount { get; set; }

    public double fPrice { get; set; }
}
