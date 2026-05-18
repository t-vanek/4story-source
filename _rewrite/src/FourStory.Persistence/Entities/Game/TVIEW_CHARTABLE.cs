using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TVIEW_CHARTABLE
{
    public int dwCharID { get; set; }

    public int dwUserID { get; set; }

    public string szName { get; set; } = null!;

    public byte bClass { get; set; }

    public byte bRace { get; set; }

    public byte bCountry { get; set; }

    public byte bSex { get; set; }

    public byte bLevel { get; set; }
}
