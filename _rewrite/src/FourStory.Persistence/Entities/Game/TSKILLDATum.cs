using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TSKILLDATum
{
    public short wSkillID { get; set; }

    public byte bAction { get; set; }

    public byte bType { get; set; }

    public byte bAttr { get; set; }

    public byte bExec { get; set; }

    public byte bInc { get; set; }

    public short wValue { get; set; }

    public short wValueInc { get; set; }

    public byte bCalc { get; set; }
}
