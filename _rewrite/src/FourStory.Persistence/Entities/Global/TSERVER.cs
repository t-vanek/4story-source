using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TSERVER
{
    public byte bGroupID { get; set; }

    public byte bServerID { get; set; }

    public byte bType { get; set; }

    public byte bMachineID { get; set; }

    public short wPort { get; set; }

    public string szName { get; set; } = null!;

    public virtual TGROUP bGroup { get; set; } = null!;

    public virtual TMACHINE bMachine { get; set; } = null!;
}
