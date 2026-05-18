using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TPORTALCHART
{
    public short wPortalID { get; set; }

    public byte bCountry { get; set; }

    public short wLocalID { get; set; }

    public short wSpawnID { get; set; }

    public byte bCondition { get; set; }
}
