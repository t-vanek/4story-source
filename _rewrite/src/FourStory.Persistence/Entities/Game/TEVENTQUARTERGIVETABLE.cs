using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TEVENTQUARTERGIVETABLE
{
    public byte bDay { get; set; }

    public byte bHour { get; set; }

    public byte bMinute { get; set; }

    public string szName { get; set; } = null!;

    public DateTime dGiveDate { get; set; }
}
