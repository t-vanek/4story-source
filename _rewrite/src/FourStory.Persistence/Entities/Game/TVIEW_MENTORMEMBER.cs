using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TVIEW_MENTORMEMBER
{
    public int dwCharID { get; set; }

    public int dwMentorID { get; set; }

    public int dwExp { get; set; }

    public string szNAME { get; set; } = null!;

    public byte bLevel { get; set; }

    public byte bClass { get; set; }

    public byte bRace { get; set; }

    public byte bSex { get; set; }

    public byte bFace { get; set; }

    public byte bHair { get; set; }

    public DateTime dLogoutDate { get; set; }
}
