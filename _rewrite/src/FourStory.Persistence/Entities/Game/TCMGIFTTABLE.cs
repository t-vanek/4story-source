using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TCMGIFTTABLE
{
    public int dwUserID { get; set; }

    public int dwCharID { get; set; }

    public short wGiftID { get; set; }

    public int dwGMCharID { get; set; }

    public short wErrID { get; set; }

    public DateTime tTakeDate { get; set; }
}
