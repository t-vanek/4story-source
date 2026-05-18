using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TINVENTABLE
{
    public int dwCharID { get; set; }

    public byte bInvenID { get; set; }

    public short wItemID { get; set; }

    public DateTime dEndTime { get; set; }

    public byte bELD { get; set; }
}
