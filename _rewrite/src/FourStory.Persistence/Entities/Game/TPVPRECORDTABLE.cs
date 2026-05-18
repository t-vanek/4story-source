using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TPVPRECORDTABLE
{
    public int dwCharID { get; set; }

    public int dwWarrior_win { get; set; }

    public int dwWarrior_lose { get; set; }

    public int dwRanger_win { get; set; }

    public int dwRanger_lose { get; set; }

    public int dwArcher_win { get; set; }

    public int dwArcher_lose { get; set; }

    public int dwWizard_win { get; set; }

    public int dwWizard_lose { get; set; }

    public int dwPriest_win { get; set; }

    public int dwPriest_lose { get; set; }

    public int dwSorcerer_win { get; set; }

    public int dwSorcerer_lose { get; set; }
}
