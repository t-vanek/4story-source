using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TLASTMONTHPOINTTABLE
{
    public int dwCharID { get; set; }

    public int dwRank { get; set; }

    public int dwPoint { get; set; }

    public short wWin { get; set; }

    public short wLose { get; set; }
}
