using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class SKILLLINK
{
    public int dwCharID { get; set; }

    public short wSkillID { get; set; }

    public byte bLevel { get; set; }

    public byte bMaxLevel { get; set; }
}
