using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TFRIENDGROUPTABLE
{
    public int dwCharID { get; set; }

    public byte bGroup { get; set; }

    public string szName { get; set; } = null!;
}
