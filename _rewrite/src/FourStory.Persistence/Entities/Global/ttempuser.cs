using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class ttempuser
{
    public int dwUserID { get; set; }

    public int dwCharID { get; set; }

    public byte bLevel { get; set; }

    public int bmaxlevel { get; set; }
}
