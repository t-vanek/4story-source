using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TGUILDWANTEDTABLE
{
    public int dwGuildID { get; set; }

    public byte bMaxLevel { get; set; }

    public byte bMinLevel { get; set; }

    public DateTime dEndTime { get; set; }

    public string szTitle { get; set; } = null!;

    public string szText { get; set; } = null!;
}
