using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TQCONDITIONCHART
{
    public int dwID { get; set; }

    public int dwQuestID { get; set; }

    public byte bConditionType { get; set; }

    public int dwConditionID { get; set; }

    public byte bCount { get; set; }
}
