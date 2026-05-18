using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TSKILLPOINTCHART
{
    public short wID { get; set; }

    public byte bLevel { get; set; }

    public byte bSkillPoint { get; set; }

    public byte bGroupPoint { get; set; }

    public byte bPrevSkillLevel { get; set; }

    public int dwPayback { get; set; }
}
