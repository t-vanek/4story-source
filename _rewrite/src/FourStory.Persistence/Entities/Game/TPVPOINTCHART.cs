using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TPVPOINTCHART
{
    public short wLocalID { get; set; }

    public byte bStatus { get; set; }

    public byte bEvent { get; set; }

    public byte bTarget { get; set; }

    public int dwIncPoint { get; set; }

    public int dwDecPoint { get; set; }
}
