using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TSPAWNPOSCHART
{
    public short wID { get; set; }

    public short wMapID { get; set; }

    public float fPosX { get; set; }

    public float fPosY { get; set; }

    public float fPosZ { get; set; }

    public byte bType { get; set; }
}
