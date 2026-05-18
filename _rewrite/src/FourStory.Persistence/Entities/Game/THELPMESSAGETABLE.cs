using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class THELPMESSAGETABLE
{
    public byte bID { get; set; }

    public DateTime dStart { get; set; }

    public DateTime dEnd { get; set; }

    public string szMessage { get; set; } = null!;
}
