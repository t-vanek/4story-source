using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TGAMBLECHART
{
    public byte bType { get; set; }

    public byte bKind { get; set; }

    public short wReplaceID { get; set; }

    public byte bCountMax { get; set; }

    public byte bMinLevel { get; set; }

    public byte bMaxLevel { get; set; }

    public short wProb { get; set; }
}
