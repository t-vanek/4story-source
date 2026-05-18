using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TITEMMAGICSKILLCHART
{
    public byte bMagic { get; set; }

    public int dwKind { get; set; }

    public short wSkillID { get; set; }

    public byte bIsMagic { get; set; }

    public byte bIsRare { get; set; }

    public byte bMinLevel { get; set; }
}
