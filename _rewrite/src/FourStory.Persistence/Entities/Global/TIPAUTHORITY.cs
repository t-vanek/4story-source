using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TIPAUTHORITY
{
    public string szIP { get; set; } = null!;

    public byte bAuthority { get; set; }
}
