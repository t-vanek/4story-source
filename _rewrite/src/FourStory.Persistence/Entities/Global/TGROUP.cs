using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TGROUP
{
    public byte bGroupID { get; set; }

    public byte bType { get; set; }

    public string szNAME { get; set; } = null!;

    public string szDSN { get; set; } = null!;

    public string szUserID { get; set; } = null!;

    public string? szPasswd { get; set; }

    public short wFull { get; set; }

    public short wBusy { get; set; }

    public int dwMaxUser { get; set; }

    public byte bUseRate { get; set; }

    public byte bStatus { get; set; }

    public virtual ICollection<TCHANNEL> TCHANNELs { get; set; } = new List<TCHANNEL>();
}
