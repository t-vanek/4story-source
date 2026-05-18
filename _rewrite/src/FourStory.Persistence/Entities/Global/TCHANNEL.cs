using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TCHANNEL
{
    public byte bGroupID { get; set; }

    public byte bChannel { get; set; }

    public string szNAME { get; set; } = null!;

    public short wFull { get; set; }

    public short wBusy { get; set; }

    public byte bStatus { get; set; }

    public virtual TGROUP bGroup { get; set; } = null!;
}
