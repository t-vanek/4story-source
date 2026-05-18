using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TSECURECODE
{
    public string? strSecurityCode { get; set; }

    public int? bEnabled { get; set; }

    public int? bTries { get; set; }

    public int? iLockTick { get; set; }

    public int? dwUserID { get; set; }
}
