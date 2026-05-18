using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TDESTINATIONCHART
{
    public short wPortalID { get; set; }

    public short wDestID { get; set; }

    public int dwPrice { get; set; }

    public byte bEnable { get; set; }

    public byte bCondition1 { get; set; }

    public int dwConditionID1 { get; set; }

    public byte bCondition2 { get; set; }

    public int dwConditionID2 { get; set; }

    public byte bCondition3 { get; set; }

    public int dwConditionID3 { get; set; }
}
