using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TGATECHART
{
    public int dwGateID { get; set; }

    public int dwSwitchID { get; set; }

    public byte bType { get; set; }

    public short wMapID { get; set; }

    public short wPosX { get; set; }

    public short wPosY { get; set; }

    public short wPosZ { get; set; }
}
