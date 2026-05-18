using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TSMSTABLE
{
    public int dwSeq { get; set; }

    public byte bWorld { get; set; }

    public int dwUserID { get; set; }

    public string szCharName { get; set; } = null!;

    public byte bType { get; set; }

    public string szSender { get; set; } = null!;

    public string szMessage { get; set; } = null!;

    public DateTime dateSend { get; set; }

    public int? dwSMS { get; set; }

    public string? bResult { get; set; }
}
