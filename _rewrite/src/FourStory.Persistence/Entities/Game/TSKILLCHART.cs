using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TSKILLCHART
{
    public short wID { get; set; }

    public string szName { get; set; } = null!;

    public short wPrevActiveID { get; set; }

    public short wParentSkillID { get; set; }

    public short wItemID { get; set; }

    public short wMaxRange { get; set; }

    public short wMinRange { get; set; }

    public short wPosture { get; set; }

    public int dwConditionID { get; set; }

    public int dwWeaponID { get; set; }

    public int dwClassID { get; set; }

    public byte bKind { get; set; }

    public double fPrice { get; set; }

    public int dwUseMP { get; set; }

    public byte bUseMPType { get; set; }

    public int dwUseHP { get; set; }

    public byte bUseHPType { get; set; }

    public int dwReuseDelay { get; set; }

    public int nReuseDelayInc { get; set; }

    public int dwLoopDelay { get; set; }

    public int dwActionTime { get; set; }

    public int dwDuration { get; set; }

    public int dwDurationInc { get; set; }

    public int dwKindDelay { get; set; }

    public int dwAggro { get; set; }

    public int dwAggroInc { get; set; }

    public byte bLevel { get; set; }

    public byte bMaxLevel { get; set; }

    public byte bNextLevel { get; set; }

    public byte bTarget { get; set; }

    public byte bTargetRange { get; set; }

    public byte bIsuse { get; set; }

    public byte bTargetHit { get; set; }

    public byte bPositive { get; set; }

    public byte bPriority { get; set; }

    public byte bSpeedApply { get; set; }

    public byte bCanLearn { get; set; }

    public byte bORadius { get; set; }

    public byte bIsRide { get; set; }

    public byte bIsDismount { get; set; }

    public short wTargetActiveID { get; set; }

    public byte bMaintainType { get; set; }

    public byte bDuraSlot { get; set; }

    public byte bCanCancel { get; set; }

    public byte bHitTest { get; set; }

    public byte bHitInit { get; set; }

    public byte bHitInc { get; set; }

    public byte bGlobal { get; set; }

    public byte bRadius { get; set; }

    public byte bStatic { get; set; }

    public byte bEraseAct { get; set; }

    public byte bEraseHide { get; set; }

    public byte bIsHideSkill { get; set; }

    public byte bRunFromServer { get; set; }

    public byte bCheckAttacker { get; set; }

    public short wTriggerID { get; set; }

    public short wMapID { get; set; }

    public byte? bRepeatCount { get; set; }
}
