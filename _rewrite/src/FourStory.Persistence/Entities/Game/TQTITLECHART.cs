using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TQTITLECHART
{
    public int dwQuestID { get; set; }

    public int dwClassID { get; set; }

    public string? szTitle { get; set; }

    public string? szMessage { get; set; }

    public string? szComplete { get; set; }

    public string? szAccept { get; set; }

    public string? szReject { get; set; }

    public string? szSummary { get; set; }

    public string? szNPCName { get; set; }

    public string? szReply { get; set; }
}
