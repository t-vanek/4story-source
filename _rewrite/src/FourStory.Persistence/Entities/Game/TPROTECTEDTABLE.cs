using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TPROTECTEDTABLE
{
    public int dwCharID { get; set; }

    public int dwProtected { get; set; }

    public string szNAME { get; set; } = null!;

    public byte bOption { get; set; }
}
