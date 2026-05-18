using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TGUILDTABLE_copy
{
    public int dwID { get; set; }

    public string szName { get; set; } = null!;

    public int dwChief { get; set; }

    public byte bLevel { get; set; }

    public int dwFame { get; set; }

    public int dwFameColor { get; set; }

    public byte bMaxCabinet { get; set; }

    public int dwGold { get; set; }

    public int dwSilver { get; set; }

    public int dwCooper { get; set; }

    public int dwGI { get; set; }

    public int dwExp { get; set; }

    public byte bGPoint { get; set; }

    public byte bStatus { get; set; }

    public byte bDisorg { get; set; }

    public int dwTime { get; set; }

    public DateTime timeEstablish { get; set; }

    public int dwPvPTotalPoint { get; set; }

    public int dwPvPUseablePoint { get; set; }

    public int dwPvPMonthPoint { get; set; }
}
