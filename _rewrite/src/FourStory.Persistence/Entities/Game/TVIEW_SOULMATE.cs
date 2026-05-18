using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TVIEW_SOULMATE
{
    public int dwCharID { get; set; }

    public int dwTarget { get; set; }

    public string szNAME { get; set; } = null!;

    public byte bLevel { get; set; }

    public byte bClass { get; set; }

    public int dwTime { get; set; }
}
