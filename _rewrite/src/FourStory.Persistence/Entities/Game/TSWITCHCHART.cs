using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TSWITCHCHART
{
    public int dwSwitchID { get; set; }

    public short wMapID { get; set; }

    public short wPosX { get; set; }

    public short wPosY { get; set; }

    public short wPosZ { get; set; }

    public byte bStart { get; set; }

    public byte bLockOnOpen { get; set; }

    public byte bLockOnClose { get; set; }

    public int dwDuration { get; set; }
}
