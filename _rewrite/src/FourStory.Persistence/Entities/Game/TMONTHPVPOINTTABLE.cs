using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TMONTHPVPOINTTABLE
{
    public int dwCharID { get; set; }

    public byte bCountry { get; set; }

    public int dwPoint { get; set; }

    public short wWin { get; set; }

    public short wLose { get; set; }

    public string szSay { get; set; } = null!;
}
