using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TLOCALTABLE
{
    public short wLocalID { get; set; }

    public byte bCountry { get; set; }

    public int dwGuild { get; set; }

    public DateTime dateOccupy { get; set; }

    public DateTime dateDefend { get; set; }

    public string szHero { get; set; } = null!;

    public DateTime dateHero { get; set; }
}
