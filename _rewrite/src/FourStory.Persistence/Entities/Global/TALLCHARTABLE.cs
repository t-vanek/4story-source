using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TALLCHARTABLE
{
    public int dwSeq { get; set; }

    public int dwUserID { get; set; }

    public byte bWorldID { get; set; }

    public int dwCharID { get; set; }

    public byte bSlot { get; set; }

    public string szName { get; set; } = null!;

    public byte bClass { get; set; }

    public byte bRace { get; set; }

    public byte bCountry { get; set; }

    public byte bSex { get; set; }

    public byte bHair { get; set; }

    public byte bFace { get; set; }

    public byte bBody { get; set; }

    public byte bPants { get; set; }

    public byte bHand { get; set; }

    public byte bFoot { get; set; }

    public byte bLevel { get; set; }

    public int dwEXP { get; set; }

    public byte bDelete { get; set; }

    public DateTime dCreateDate { get; set; }

    public DateTime? dDeleteDate { get; set; }

    public DateTime? dLoginDate { get; set; }

    public DateTime? dLogoutDate { get; set; }

    public int dwPlayTime { get; set; }

    public int dwGold { get; set; }

    public int dwSilver { get; set; }

    public int dwCooper { get; set; }

    public int? isOnline { get; set; }

    public int? Ban { get; set; }

    public string? BanReason { get; set; }
}
