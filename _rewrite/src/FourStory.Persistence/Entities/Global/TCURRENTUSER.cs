using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TCURRENTUSER
{
    public int dwKEY { get; set; }

    public int dwUserID { get; set; }

    public int dwCharID { get; set; }

    public byte bGroupID { get; set; }

    public byte bChannel { get; set; }

    public string? szIPAddr { get; set; }

    public short wPort { get; set; }

    public byte bLocked { get; set; }

    public DateTime dLoginDate { get; set; }

    public DateTime dEnterDate { get; set; }

    public int dwPcBangID { get; set; }

    public byte bLuckyNumber { get; set; }

    public string szLoginIP { get; set; } = null!;
}
