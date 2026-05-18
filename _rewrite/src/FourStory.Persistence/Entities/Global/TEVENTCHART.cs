using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TEVENTCHART
{
    public int dwIndex { get; set; }

    public byte bID { get; set; }

    public string szTitle { get; set; } = null!;

    public byte bGroupID { get; set; }

    public byte bSvrType { get; set; }

    public byte bSvrID { get; set; }

    public DateTime dStartDate { get; set; }

    public DateTime dEndDate { get; set; }

    public short wValue { get; set; }

    public int dwStartAlarm { get; set; }

    public int dwEndAlarm { get; set; }

    public string szStartMsg { get; set; } = null!;

    public string szEndMsg { get; set; } = null!;

    public string szValue { get; set; } = null!;

    public short wMapID { get; set; }

    public string szMidMsg { get; set; } = null!;

    public byte bPartTime { get; set; }
}
