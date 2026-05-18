using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Global;

public partial class TUSERINFOTABLE
{
    public int dwUserID { get; set; }

    public byte bCanCreateCharCount { get; set; }

    public byte bAgreement { get; set; }

    public DateTime? dCabinetUse { get; set; }
}
