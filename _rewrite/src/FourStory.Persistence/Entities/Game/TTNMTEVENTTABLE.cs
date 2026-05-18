using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TTNMTEVENTTABLE
{
    public short wTournamentID { get; set; }

    public byte bEntryID { get; set; }

    public string szName { get; set; } = null!;

    public byte bType { get; set; }

    public int dwClass { get; set; }

    public int dwFee { get; set; }

    public int dwFeeBack { get; set; }

    public short wItemID { get; set; }

    public byte bItemCount { get; set; }

    public byte bMaxLevel { get; set; }

    public byte bMinLevel { get; set; }
}
