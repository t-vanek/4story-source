using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TALLCHARVIEW
{
    public int dwUserID { get; set; }

    public byte bWorldID { get; set; }

    public int dwCharID { get; set; }

    public byte bSlot { get; set; }

    public string szNAME { get; set; } = null!;

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
}
