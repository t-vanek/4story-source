using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TSKILLLOG
{
    public int dwCharID { get; set; }

    public int dwGold { get; set; }

    public int dwSilver { get; set; }

    public int dwCooper { get; set; }

    public short wSkill { get; set; }

    public byte bLevel { get; set; }

    public DateTime timeInsert { get; set; }
}
