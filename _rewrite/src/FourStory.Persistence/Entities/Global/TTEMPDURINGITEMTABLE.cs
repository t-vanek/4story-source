using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TTEMPDURINGITEMTABLE
{
    public int dwUserID { get; set; }

    public short wItemID { get; set; }

    public byte bType { get; set; }

    public int dwRemainTime { get; set; }

    public DateTime dEndTime { get; set; }
}
