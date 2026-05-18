using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TGUILDPVPOINTREWARDTABLE
{
    public int dwGuildID { get; set; }

    public string szName { get; set; } = null!;

    public int dwPoint { get; set; }

    public DateTime dlDate { get; set; }
}
