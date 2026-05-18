using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TCMGIFTCHART
{
    public short wGiftID { get; set; }

    public byte bGiftType { get; set; }

    public int dwValue { get; set; }

    public byte bCount { get; set; }

    public byte bTakeType { get; set; }

    public byte bMaxTakeCount { get; set; }

    public byte bLevel { get; set; }

    public byte bToolOnly { get; set; }

    public short wErrGiftID { get; set; }

    public string szTitle { get; set; } = null!;

    public string szMsg { get; set; } = null!;
}
