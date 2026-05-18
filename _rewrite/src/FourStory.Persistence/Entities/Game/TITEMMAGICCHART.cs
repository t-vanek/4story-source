using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TITEMMAGICCHART
{
    public byte bMagic { get; set; }

    public int dwKind { get; set; }

    public byte bRvType { get; set; }

    public short wMaxValue { get; set; }

    public byte bIsMagic { get; set; }

    public byte bIsRare { get; set; }

    public byte bMinLevel { get; set; }

    public byte bExclIndex { get; set; }

    public byte bOptionKind { get; set; }

    public short wAutoSkill { get; set; }

    public byte bRefine { get; set; }

    public short wMaxBound { get; set; }

    public short wRareBound { get; set; }
}
