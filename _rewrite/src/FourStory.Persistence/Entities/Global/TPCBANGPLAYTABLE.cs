using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TPCBANGPLAYTABLE
{
    public int dwUserID { get; set; }

    public int dwPlayDate { get; set; }

    public int dwPlayTime { get; set; }

    public byte bItemCnt { get; set; }
}
