using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TMONATTRCHART
{
    public short wID { get; set; }

    public byte bLevel { get; set; }

    public short wAP { get; set; }

    public short wLAP { get; set; }

    public int dwAtkSpeed { get; set; }

    public short wAL { get; set; }

    public short wDL { get; set; }

    public byte bCriticalPP { get; set; }

    public int dwMaxHP { get; set; }

    public byte bHPRecover { get; set; }

    public short wMAP { get; set; }

    public short bCriticalMP { get; set; }

    public int dwMaxMP { get; set; }

    public byte bMPRecover { get; set; }

    public short wDP { get; set; }

    public short wMDP { get; set; }

    public short wMinWAP { get; set; }

    public short wMaxWAP { get; set; }

    public short wWDP { get; set; }

    public short wMAL { get; set; }

    public short wMDL { get; set; }
}
