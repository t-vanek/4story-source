using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TGUILDRELATION
{
    public byte bType { get; set; }

    public int dwGuildOne { get; set; }

    public int dwGuildTwo { get; set; }
}
