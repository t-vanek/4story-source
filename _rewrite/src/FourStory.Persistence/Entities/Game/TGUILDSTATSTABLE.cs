using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TGUILDSTATSTABLE
{
    public int dwGuildID { get; set; }

    public short bSkillPoint { get; set; }

    public short bLevel { get; set; }

    public int dwExp { get; set; }
}
