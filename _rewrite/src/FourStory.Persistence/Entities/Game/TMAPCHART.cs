using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TMAPCHART
{
    public byte bGroupID { get; set; }

    public short wMapID { get; set; }

    public byte bServerID { get; set; }

    public byte bChannel { get; set; }
}
