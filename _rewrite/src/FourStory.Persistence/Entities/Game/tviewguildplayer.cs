using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class tviewguildplayer
{
    public int dwCharID { get; set; }

    public int dwGuildID { get; set; }

    public string g_name { get; set; } = null!;

    public int dwID { get; set; }

    public int p_dwcharid { get; set; }

    public string p_name { get; set; } = null!;

    public byte bLevel { get; set; }

    public byte bFace { get; set; }

    public byte bSex { get; set; }

    public byte bRace { get; set; }

    public byte bHair { get; set; }

    public byte bCountry { get; set; }

    public byte bClass { get; set; }
}
