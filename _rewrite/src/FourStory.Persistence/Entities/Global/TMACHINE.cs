using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TMACHINE
{
    public byte bMachineID { get; set; }

    public string szNAME { get; set; } = null!;

    public byte bRouteID { get; set; }

    public virtual ICollection<TIPADDR> TIPADDRs { get; set; } = new List<TIPADDR>();

    public virtual TNETWORK? TNETWORK { get; set; }
}
