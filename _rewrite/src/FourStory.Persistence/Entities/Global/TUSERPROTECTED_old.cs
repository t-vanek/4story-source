using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TUSERPROTECTED_old
{
    public int dwSeq { get; set; }

    public int dwUserID { get; set; }

    public byte bBlockType { get; set; }

    public byte bEternal { get; set; }

    public byte? bWorld { get; set; }

    public int? dwCharID { get; set; }

    public string? szCharName { get; set; }

    public DateTime startTime { get; set; }

    public int dwDuration { get; set; }

    public byte bBlockReason { get; set; }

    public string szComment { get; set; } = null!;

    public string szGMID { get; set; } = null!;

    public DateTime regDate { get; set; }

    public int? bPerm { get; set; }
}
