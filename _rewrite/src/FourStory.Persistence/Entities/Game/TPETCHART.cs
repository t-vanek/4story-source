using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TPETCHART
{
    public short wID { get; set; }

    public byte bPetType { get; set; }

    public byte bRace { get; set; }

    public short wMonID { get; set; }

    public byte bRecallKind1 { get; set; }

    public byte bRecallKind2 { get; set; }

    public short wRecallValue1 { get; set; }

    public short wRecallValue2 { get; set; }

    public byte bConditionType { get; set; }

    public int dwConditionValue { get; set; }
}
