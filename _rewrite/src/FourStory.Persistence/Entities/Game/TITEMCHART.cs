using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TITEMCHART
{
    public short wItemID { get; set; }

    public byte bType { get; set; }

    public byte bKind { get; set; }

    public string szNAME { get; set; } = null!;

    public short wAttrID { get; set; }

    public short wUseValue { get; set; }

    public int dwSlotID { get; set; }

    public int dwClassID { get; set; }

    public byte bPrmSlotID { get; set; }

    public byte bSubSlotID { get; set; }

    public byte bLevel { get; set; }

    public double fPrice { get; set; }

    public byte bIsSell { get; set; }

    public byte bMinRange { get; set; }

    public byte bMaxRange { get; set; }

    public byte bStack { get; set; }

    public byte bEquipSkill { get; set; }

    public byte bSlotCount { get; set; }

    public byte bUseItemKind { get; set; }

    public byte bUseItemCount { get; set; }

    public byte bGrade { get; set; }

    public short wUseTime { get; set; }

    public byte bUseType { get; set; }

    public byte bCanGrade { get; set; }

    public byte bCanMagic { get; set; }

    public byte bCanRare { get; set; }

    public byte bDropLevel { get; set; }

    public int dwSpeedInc { get; set; }

    public byte bItemCountry { get; set; }

    public byte bIsSpecial { get; set; }

    public int dwDelay { get; set; }

    public double fRevision { get; set; }

    public double fMRevision { get; set; }

    public double fAtRate { get; set; }

    public double fMAtRate { get; set; }

    public byte bCanGamble { get; set; }

    public short wItemProb_G { get; set; }

    public byte bDestroyProb { get; set; }

    public byte bGambleProb { get; set; }

    public int dwDuraMax { get; set; }

    public byte bRefineMax { get; set; }

    public byte bCanRepair { get; set; }

    public short wDelayGroupID { get; set; }

    public short wWeight { get; set; }

    public byte bGroupID { get; set; }

    public byte bInitState { get; set; }

    public byte bCanWrap { get; set; }

    public int dwCode { get; set; }

    public byte bCanColor { get; set; }

    public double fPvPrice { get; set; }

    public byte bConsumable { get; set; }

    public short wExpandValue { get; set; }
}
