using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TPREVERSION
{
    public int dwBetaVer { get; set; }

    public string szPath { get; set; } = null!;

    public string szName { get; set; } = null!;

    public int dwSize { get; set; }
}
