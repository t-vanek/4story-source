using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TSKILLTABLE
{
    public int dwCharID { get; set; }

    public short wSkillID { get; set; }

    public byte bLevel { get; set; }

    public int dwRemainTick { get; set; }
}
