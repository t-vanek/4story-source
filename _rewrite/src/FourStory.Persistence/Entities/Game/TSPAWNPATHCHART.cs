using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TSPAWNPATHCHART
{
    public short wSpawnID { get; set; }

    public byte bPathID { get; set; }

    public float fPosX { get; set; }

    public float fPosY { get; set; }

    public float fPosZ { get; set; }

    public byte bProb { get; set; }

    public float fRadius { get; set; }
}
