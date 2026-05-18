using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TGUILDMEMBERSKILLTABLE
{
    public short? wSkillID { get; set; }

    public byte? bLevel { get; set; }

    public DateTime? tEndTime { get; set; }

    public int? dwCharID { get; set; }
}
