using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TVIEW_TOURNAMENTPLAYER
{
    public int dwCharID { get; set; }

    public string szNAME { get; set; } = null!;

    public int dwChiefID { get; set; }

    public byte bClass { get; set; }

    public byte bCountry { get; set; }

    public byte bLevel { get; set; }

    public byte bEntry { get; set; }

    public byte bStep { get; set; }

    public byte bResult { get; set; }

    public int? dwIPAddr { get; set; }

    public string? szHWID { get; set; }
}
