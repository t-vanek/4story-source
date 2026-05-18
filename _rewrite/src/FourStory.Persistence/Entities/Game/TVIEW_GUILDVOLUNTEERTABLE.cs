using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TVIEW_GUILDVOLUNTEERTABLE
{
    public byte bType { get; set; }

    public int dwCharID { get; set; }

    public string szNAME { get; set; } = null!;

    public int dwID { get; set; }

    public byte bLevel { get; set; }

    public byte bClass { get; set; }
}
