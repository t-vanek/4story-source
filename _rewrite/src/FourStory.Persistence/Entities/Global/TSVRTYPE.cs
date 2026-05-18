using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TSVRTYPE
{
    public byte bType { get; set; }

    public string szName { get; set; } = null!;

    public byte bControl { get; set; }
}
