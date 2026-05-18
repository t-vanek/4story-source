using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TCHARTABLE
{
    public int dwCharID { get; set; }

    public int dwUserID { get; set; }

    public byte bSlot { get; set; }

    public string szNAME { get; set; } = null!;

    public byte bStartAct { get; set; }

    public byte bClass { get; set; }

    public byte bRace { get; set; }

    public byte bCountry { get; set; }

    public byte bRealSex { get; set; }

    public byte bSex { get; set; }

    public byte bHair { get; set; }

    public byte bFace { get; set; }

    public byte bBody { get; set; }

    public byte bPants { get; set; }

    public byte bHand { get; set; }

    public byte bFoot { get; set; }

    public byte bHelmetHide { get; set; }

    public byte bLevel { get; set; }

    public int dwEXP { get; set; }

    public int dwHP { get; set; }

    public int dwMP { get; set; }

    public short wSkillPoint { get; set; }

    public int dwRegion { get; set; }

    public int dwGold { get; set; }

    public int dwSilver { get; set; }

    public int dwCooper { get; set; }

    public byte bGuildLeave { get; set; }

    public int dwGuildLeaveTime { get; set; }

    public short wMapID { get; set; }

    public short wSpawnID { get; set; }

    public short wLastSpawnID { get; set; }

    public short wTemptedMon { get; set; }

    public byte bAftermath { get; set; }

    public float fPosX { get; set; }

    public float fPosY { get; set; }

    public float fPosZ { get; set; }

    public short wDIR { get; set; }

    public int dwRankPoint { get; set; }

    public byte bDelete { get; set; }

    public DateTime dCreateDate { get; set; }

    public DateTime? dDeleteDate { get; set; }

    public int dwLastDestination { get; set; }

    public byte bOriCountry { get; set; }

    public DateTime dLogoutDate { get; set; }

    public byte? bStatLevel { get; set; }

    public byte? bStatPoint { get; set; }

    public int? dwStatExp { get; set; }
}
