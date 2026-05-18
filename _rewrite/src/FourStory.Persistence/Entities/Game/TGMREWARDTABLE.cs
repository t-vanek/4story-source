using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TGMREWARDTABLE
{
    public int dwUserID { get; set; }

    public byte bLevel { get; set; }

    public int dwCharID { get; set; }

    public DateTime dDate { get; set; }
}
