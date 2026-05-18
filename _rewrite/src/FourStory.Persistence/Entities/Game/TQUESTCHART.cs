using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TQUESTCHART
{
    public int dwQuestID { get; set; }

    public int dwParentID { get; set; }

    public byte bType { get; set; }

    public byte bForceRun { get; set; }

    public byte bTriggerType { get; set; }

    public int dwTriggerID { get; set; }

    public byte bCountMax { get; set; }

    public byte bLevel { get; set; }

    public byte bMain { get; set; }

    public byte bConditionCheck { get; set; }
}
