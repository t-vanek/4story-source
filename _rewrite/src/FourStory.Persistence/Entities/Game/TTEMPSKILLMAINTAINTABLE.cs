using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TTEMPSKILLMAINTAINTABLE
{
    public int dwCharID { get; set; }

    public short wSkillID { get; set; }

    public byte bLevel { get; set; }

    public int dwRemainTick { get; set; }

    public byte bAttackType { get; set; }

    public int dwAttackID { get; set; }

    public byte bHostType { get; set; }

    public int dwHostID { get; set; }

    public byte bAttackCountry { get; set; }

    public float fPosX { get; set; }

    public float fPosY { get; set; }

    public float fPosZ { get; set; }
}
