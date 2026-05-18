using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TGUILDPVPRECORDTABLE
{
    public int dwGuildID { get; set; }

    public int dwCharID { get; set; }

    public int dwDate { get; set; }

    public short wKillCount { get; set; }

    public short wDieCount { get; set; }

    public int dwPoint_1 { get; set; }

    public int dwPoint_2 { get; set; }

    public int dwPoint_3 { get; set; }

    public int dwPoint_4 { get; set; }

    public int dwPoint_5 { get; set; }

    public int dwPoint_6 { get; set; }

    public int dwPoint_7 { get; set; }

    public int dwPoint_8 { get; set; }
}
