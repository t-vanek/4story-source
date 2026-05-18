using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TGUILDMEMBERTABLE
{
    public int dwCharID { get; set; }

    public int dwGuildID { get; set; }

    public byte bDuty { get; set; }

    public byte bPeer { get; set; }

    public int dwService { get; set; }
}
