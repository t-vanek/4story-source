using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TIPADDR
{
    public byte bMachineID { get; set; }

    public string szIPAddr { get; set; } = null!;

    public string szPriAddr { get; set; } = null!;

    public byte bActive { get; set; }

    public virtual TMACHINE bMachine { get; set; } = null!;
}
