using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TBATTLETIMECHART
{
    public byte bType { get; set; }

    public int dwBattleDur { get; set; }

    public int dwBattleStart { get; set; }

    public int dwAlarmStart { get; set; }

    public int dwAlarmEnd { get; set; }

    public int dwPeaceDur { get; set; }

    public byte bDay { get; set; }

    public byte bWeek { get; set; }
}
