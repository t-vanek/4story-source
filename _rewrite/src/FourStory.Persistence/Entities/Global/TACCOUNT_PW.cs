using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TACCOUNT_PW
{
    public string szUserID { get; set; } = null!;

    public string? szPasswd { get; set; }

    public byte? bCheck { get; set; }

    public DateTime? dFirstLogin { get; set; }

    public DateTime? dLastLogin { get; set; }

    public int dwUserID { get; set; }

    public int? dwChanged { get; set; }

    public string? dwPWKey { get; set; }

    public string? szRealUsername { get; set; }

    public string? szOrigin { get; set; }

    public string? dwEMKey { get; set; }
}
