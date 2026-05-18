using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TDURINGITEMTABLE
{
    public int dwUserID { get; set; }

    public short wItemID { get; set; }

    public byte bType { get; set; }

    public int dwRemainTime { get; set; }

    public DateTime dEndTime { get; set; }
}
