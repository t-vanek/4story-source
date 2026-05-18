using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TCHANNELCHART
{
    public byte bGroupID { get; set; }

    public short wMapID { get; set; }

    public short wUnitID { get; set; }

    public byte bLogChannel { get; set; }

    public byte bPhyChannel { get; set; }
}
