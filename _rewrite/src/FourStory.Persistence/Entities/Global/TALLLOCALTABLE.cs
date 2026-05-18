using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TALLLOCALTABLE
{
    public DateTime dDate { get; set; }

    public byte bWorld { get; set; }

    public short wLocalID { get; set; }

    public string szLocalName { get; set; } = null!;

    public byte? bCountry { get; set; }

    public int? dwGuild { get; set; }

    public string? szGuildName { get; set; }

    public DateTime? dOccupy { get; set; }

    public DateTime? dDefend { get; set; }
}
