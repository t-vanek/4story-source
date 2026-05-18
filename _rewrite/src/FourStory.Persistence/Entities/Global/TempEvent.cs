using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TempEvent
{
    public int? dwUserID { get; set; }

    public DateTime? CheckDate { get; set; }

    public int? dwPlayTime { get; set; }

    public DateTime? timeStart { get; set; }

    public DateTime? timeEnd { get; set; }
}
