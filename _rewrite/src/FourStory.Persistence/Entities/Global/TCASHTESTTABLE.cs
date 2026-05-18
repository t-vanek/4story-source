using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TCASHTESTTABLE
{
    public int dwUserID { get; set; }

    public int dwCash { get; set; }

    public int dwBonus { get; set; }

    public DateTime? isPosted { get; set; }

    public string? byWho { get; set; }
}
