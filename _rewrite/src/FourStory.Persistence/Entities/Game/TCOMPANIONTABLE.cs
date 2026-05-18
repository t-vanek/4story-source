using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TCOMPANIONTABLE
{
    public byte bSlot { get; set; }

    public int dwMonID { get; set; }

    public byte bLevel { get; set; }

    public string strName { get; set; } = null!;

    public int dwExp { get; set; }

    public short wLife { get; set; }

    public byte bStatusPoints { get; set; }

    public byte bEffect { get; set; }

    public short wSTR { get; set; }

    public short wDEX { get; set; }

    public short wCON { get; set; }

    public short wINT { get; set; }

    public short wWIS { get; set; }

    public short wMEN { get; set; }

    public short wBonusID { get; set; }

    public int dwCharID { get; set; }
}
