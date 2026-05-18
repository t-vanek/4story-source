using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TMONSTERCHART
{
    public short wID { get; set; }

    public string szName { get; set; } = null!;

    public byte bRace { get; set; }

    public byte bClass { get; set; }

    public short wKind { get; set; }

    public byte bLevel { get; set; }

    public byte bAIType { get; set; }

    public byte bRange { get; set; }

    public short wChaseRange { get; set; }

    public byte bRoamProb { get; set; }

    public byte bMoneyProb { get; set; }

    public int dwMinMoney { get; set; }

    public int dwMaxMoney { get; set; }

    public byte bItemProb { get; set; }

    public byte bDropCount { get; set; }

    public short wExp { get; set; }

    public byte bIsSelf { get; set; }

    public byte bRecallType { get; set; }

    public byte bCanSelect { get; set; }

    public byte bCanAttack { get; set; }

    public byte bTame { get; set; }

    public byte bCall { get; set; }

    public byte bIsSpecial { get; set; }

    public byte bRemove { get; set; }

    public short wMonAttr { get; set; }

    public short wSummonAttr { get; set; }

    public short wTransSkillID { get; set; }

    public double fSize { get; set; }

    public short wSkill1 { get; set; }

    public short wSkill2 { get; set; }

    public short wSkill3 { get; set; }

    public short wSkill4 { get; set; }
}
