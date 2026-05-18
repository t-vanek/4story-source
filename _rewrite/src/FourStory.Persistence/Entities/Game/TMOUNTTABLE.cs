using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TMOUNTTABLE
{
    public int dwCharID { get; set; }

    public short wMountID { get; set; }

    public string szName { get; set; } = null!;

    public DateTime dEndTime { get; set; }
}
