using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TNETWORK
{
    public byte bMachineID { get; set; }

    public string szNetwork { get; set; } = null!;

    public virtual TMACHINE bMachine { get; set; } = null!;
}
