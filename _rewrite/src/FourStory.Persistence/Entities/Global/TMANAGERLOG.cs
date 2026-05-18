using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TMANAGERLOG
{
    public int dwSeq { get; set; }

    public DateTime dDate { get; set; }

    public string szIP { get; set; } = null!;

    public string szGMID { get; set; } = null!;

    public string szCommand { get; set; } = null!;

    public string szLog { get; set; } = null!;
}
