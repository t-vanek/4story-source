using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TMANAGER
{
    public string szID { get; set; } = null!;

    public string szPasswd { get; set; } = null!;

    public byte bOPAuthority { get; set; }

    public byte bAuthority { get; set; }

    public string? szName { get; set; }

    public string? szPhoneNum { get; set; }

    public string? szOpratorCharID { get; set; }

    public DateTime? dCreateDate { get; set; }
}
