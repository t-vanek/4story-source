using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TCASTLETABLE
{
    public short wCastle { get; set; }

    public byte bCountry { get; set; }

    public int dwGuildID { get; set; }

    public DateTime dateWarTime { get; set; }

    public string szHero { get; set; } = null!;

    public DateTime dateHero { get; set; }
}
