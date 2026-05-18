using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TBOWSETTINGSCHART
{
    public short wMapID { get; set; }

    public byte bMinPlayersCount { get; set; }

    public byte bMaxNationDifference { get; set; }

    public int dwAlarmDur { get; set; }

    public int dwBuyTimeDur { get; set; }

    public int dwBattleDur { get; set; }
}
