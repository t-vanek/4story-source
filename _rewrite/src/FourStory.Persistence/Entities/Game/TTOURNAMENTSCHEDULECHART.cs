using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TTOURNAMENTSCHEDULECHART
{
    public byte bStep { get; set; }

    public string szName { get; set; } = null!;

    public int dwPeriod { get; set; }

    public byte bGroup { get; set; }
}
