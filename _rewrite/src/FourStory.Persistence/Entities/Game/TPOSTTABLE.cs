using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TPOSTTABLE
{
    public int dwCharID { get; set; }

    public int dwPostID { get; set; }

    public string szRecvName { get; set; } = null!;

    public byte bType { get; set; }

    public byte bRead { get; set; }

    public DateTime timeRecv { get; set; }

    public int dwSendID { get; set; }

    public string szSender { get; set; } = null!;

    public string szTitle { get; set; } = null!;

    public string szMessage { get; set; } = null!;

    public int dwGold { get; set; }

    public int dwSilver { get; set; }

    public int dwCooper { get; set; }

    public byte? bContain { get; set; }
}
