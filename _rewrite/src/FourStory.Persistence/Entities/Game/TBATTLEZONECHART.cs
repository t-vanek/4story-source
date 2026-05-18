using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TBATTLEZONECHART
{
    public short wID { get; set; }

    public string szName { get; set; } = null!;

    public short wMapID { get; set; }

    public short wCastle { get; set; }

    public short wBossSpawnID { get; set; }

    public short wLGateKeeperSpawnID { get; set; }

    public short wRGateKeeperSpawnID { get; set; }

    public int dwLSwitchID { get; set; }

    public int dwRSwitchID { get; set; }

    public short wNormalItem { get; set; }

    public short wChiefItem { get; set; }

    public byte bLine { get; set; }

    public short wCGateKeeperSpawnID { get; set; }

    public int dwCSwitchID { get; set; }

    public short wSkill1 { get; set; }

    public short wSkill2 { get; set; }

    public byte bItemLevel { get; set; }

    public short wValorianBossDefend { get; set; }

    public short wValorianBossAttack { get; set; }

    public short wDerionBossDefend { get; set; }

    public short wDerionBossAttack { get; set; }

    public short wMiddleSpawnID { get; set; }

    public short wRightSpawnID { get; set; }

    public short wLeftSpawnID { get; set; }
}
