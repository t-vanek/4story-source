using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TGUILDPLAYLOG
{
    public int dwGuildID { get; set; }

    public int dwUserID { get; set; }

    public int dwCharID { get; set; }

    public int dwPlayTime { get; set; }
}
