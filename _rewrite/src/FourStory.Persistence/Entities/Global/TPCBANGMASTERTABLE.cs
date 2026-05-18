using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TPCBANGMASTERTABLE
{
    public int dwPcBangID { get; set; }

    public int dwUserID { get; set; }

    public DateTime? dWorld1 { get; set; }

    public DateTime? dWorld2 { get; set; }

    public DateTime? dWorld3 { get; set; }

    public DateTime? dWorld4 { get; set; }
}
