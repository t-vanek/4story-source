using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TRESERVEDNAME
{
    public int dwSeq { get; set; }

    public int dwUserID { get; set; }

    public string szName { get; set; } = null!;
}
