using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TTOURNAMENTPLAYERTABLE
{
    public int dwCharID { get; set; }

    public int dwChiefID { get; set; }

    public byte bEntry { get; set; }

    public byte bStep { get; set; }

    public byte bResult { get; set; }

    public string? szHWID { get; set; }

    public int? dwIPAddr { get; set; }
}
