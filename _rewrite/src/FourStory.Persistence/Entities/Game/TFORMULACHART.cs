using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TFORMULACHART
{
    public byte bID { get; set; }

    public string szName { get; set; } = null!;

    public int dwinit { get; set; }

    public double fRateX { get; set; }

    public double fRateY { get; set; }
}
