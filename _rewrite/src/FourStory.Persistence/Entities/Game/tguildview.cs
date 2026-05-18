using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class tguildview
{
    public int guild_ID { get; set; }

    public byte guild_level { get; set; }

    public string guild_name { get; set; } = null!;

    public int guild_leader_ID { get; set; }

    public int total_honor { get; set; }

    public int month_honor { get; set; }

    public int char_charid { get; set; }

    public string guild_leader { get; set; } = null!;

    public byte char_country { get; set; }
}
