using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TQREWARDCHART
{
    public int dwID { get; set; }

    public int dwQuestID { get; set; }

    public byte bRewardType { get; set; }

    public int dwRewardID { get; set; }

    public byte bTakeMethod { get; set; }

    public byte bTakeData { get; set; }

    public byte bCount { get; set; }

    public int dwQuestMob { get; set; }

    public int dwQuestTime { get; set; }

    public int dwQuestPathMob { get; set; }

    public int dwTicketID { get; set; }

    public byte bSendQ { get; set; }
}
