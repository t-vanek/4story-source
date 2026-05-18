using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TGUILDARTICLETABLE
{
    public int dwGuildID { get; set; }

    public int dwID { get; set; }

    public byte bDuty { get; set; }

    public string szWritter { get; set; } = null!;

    public string szTitle { get; set; } = null!;

    public string szArticle { get; set; } = null!;

    public int dwTime { get; set; }
}
