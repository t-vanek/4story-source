using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TQUESTTERMTABLE
{
    public int dwCharID { get; set; }

    public int dwQuestID { get; set; }

    public int dwTermID { get; set; }

    public byte bTermType { get; set; }

    public byte bCount { get; set; }
}
