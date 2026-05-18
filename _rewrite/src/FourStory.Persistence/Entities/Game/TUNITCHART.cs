using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TUNITCHART
{
    public byte bGroup { get; set; }

    public byte bServerID { get; set; }

    public short wMapID { get; set; }

    public short wUnitID { get; set; }
}
