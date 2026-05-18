using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TQUESTTABLE
{
    public int dwCharID { get; set; }

    public int dwQuestID { get; set; }

    public int dwTick { get; set; }

    public byte bCompleteCount { get; set; }

    public byte bTriggerCount { get; set; }
}
