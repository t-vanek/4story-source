using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TEVENTQUARTERCHART
{
    public byte bDay { get; set; }

    public byte bHour { get; set; }

    public byte bMinute { get; set; }

    public short wID { get; set; }

    public short wItemID1 { get; set; }

    public short wItemID2 { get; set; }

    public short wItemID3 { get; set; }

    public short wItemID4 { get; set; }

    public short wItemID5 { get; set; }

    public byte bCount { get; set; }

    public string szTitle { get; set; } = null!;

    public string szMessage { get; set; } = null!;

    public string szPresent { get; set; } = null!;

    public string szAnnounce { get; set; } = null!;
}
