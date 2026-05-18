using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TLOG
{
    public int dwKEY { get; set; }

    public int dwUserID { get; set; }

    public int dwCharID { get; set; }

    public byte bGroupID { get; set; }

    public byte bChannel { get; set; }

    public DateTime timeLOGIN { get; set; }

    public DateTime timeLOGOUT { get; set; }
}
