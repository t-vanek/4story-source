using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TPETTABLE
{
    public int dwUserID { get; set; }

    public short wPetID { get; set; }

    public string szName { get; set; } = null!;

    public DateTime timeUse { get; set; }

    public byte? bEffect { get; set; }
}
