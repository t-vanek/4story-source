using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TALLCHARTABLE_TRIGGER
{
    public string szDBOP { get; set; } = null!;

    public int dwSeq { get; set; }

    public DateTime dOPDate { get; set; }
}
