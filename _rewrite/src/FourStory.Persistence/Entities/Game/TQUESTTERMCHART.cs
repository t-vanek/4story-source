using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TQUESTTERMCHART
{
    public int dwID { get; set; }

    public int dwQuestID { get; set; }

    public byte bTermType { get; set; }

    public int dwTermID { get; set; }

    public byte bCount { get; set; }
}
