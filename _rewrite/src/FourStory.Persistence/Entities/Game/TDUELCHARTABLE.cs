using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TDUELCHARTABLE
{
    public int dwCharID { get; set; }

    public string szName { get; set; } = null!;

    public byte bClass { get; set; }

    public byte bLevel { get; set; }

    public byte bWin { get; set; }

    public int dwPoint { get; set; }

    public DateTime dTime { get; set; }
}
