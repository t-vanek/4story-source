using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TGUILDMEMBER
{
    public int dwCharID { get; set; }

    public int dwGuildID { get; set; }

    public string szNAME { get; set; } = null!;

    public byte bLevel { get; set; }

    public byte bClass { get; set; }

    public byte bDuty { get; set; }

    public byte bPeer { get; set; }

    public int dwService { get; set; }

    public DateTime dLogoutDate { get; set; }

    public byte bCountry { get; set; }

    public byte? bWarCountry { get; set; }
}
