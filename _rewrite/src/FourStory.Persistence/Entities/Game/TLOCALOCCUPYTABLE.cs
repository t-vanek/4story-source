using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TLOCALOCCUPYTABLE
{
    public short wLocalID { get; set; }

    public byte bDay { get; set; }

    public int dwGuildID { get; set; }

    public byte bType { get; set; }
}
