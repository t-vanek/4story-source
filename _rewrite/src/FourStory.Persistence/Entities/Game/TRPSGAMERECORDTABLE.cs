using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TRPSGAMERECORDTABLE
{
    public byte bType { get; set; }

    public byte bWinCount { get; set; }

    public DateTime dWinDate { get; set; }

    public int dwCharID { get; set; }
}
