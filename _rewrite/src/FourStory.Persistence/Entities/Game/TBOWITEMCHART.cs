using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TBOWITEMCHART
{
    public byte bClass { get; set; }

    public byte bInvenID { get; set; }

    public short wMinItemID { get; set; }

    public short wMaxItemID { get; set; }

    public short wDependOnItemID { get; set; }

    public byte bCount { get; set; }
}
