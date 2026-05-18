using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TRECALLMAINTAINTABLE
{
    public int dwCharID { get; set; }

    public int dwRecallID { get; set; }

    public short wSkillID { get; set; }

    public byte bLevel { get; set; }

    public int dwRemainTick { get; set; }

    public byte bAttackType { get; set; }

    public int dwAttackID { get; set; }

    public string bHostType { get; set; } = null!;

    public int dwHostID { get; set; }

    public byte bAttackCountry { get; set; }
}
