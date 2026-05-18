using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TMAPMONCHART
{
    public short wSpawnID { get; set; }

    public short wMonID { get; set; }

    public byte bEssential { get; set; }

    public byte bLeader { get; set; }

    public byte bProb { get; set; }
}
