using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TACCOUNT
{
    public int dwUserID { get; set; }

    public string? szUserID { get; set; }

    public string? szPasswd { get; set; }

    public byte? bCheck { get; set; }

    public DateTime? dFirstLogin { get; set; }

    public DateTime? dLastLogin { get; set; }
}
