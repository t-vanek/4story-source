using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TTNMTEVENTTIMETABLE
{
    public short wTournamentID { get; set; }

    public byte bWeek { get; set; }

    public byte bDay { get; set; }

    public int dwStart { get; set; }
}
