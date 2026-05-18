using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TCOMPANIONITEMTABLE
{
    public byte bSlot { get; set; }

    public short wFirstItemID { get; set; }

    public short wSecondItemID { get; set; }

    public DateTime dFirstEndTime { get; set; }

    public DateTime dSecondEndTime { get; set; }

    public int dwTick { get; set; }

    public int dwCharID { get; set; }
}
