using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TTITLECHART
{
    public short wTitleID { get; set; }

    public short wCategory { get; set; }

    public string strTitle { get; set; } = null!;

    public byte bKind { get; set; }

    public int dwRequirement { get; set; }
}
