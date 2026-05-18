using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TAICHART
{
    public byte bAIType { get; set; }

    public int dwCmdID { get; set; }

    public byte bTriggerType { get; set; }

    public int dwTriggerID { get; set; }

    public int dwDelay { get; set; }

    public byte bLoop { get; set; }
}
