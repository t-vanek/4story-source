using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TMONSPAWNCHART
{
    public short wID { get; set; }

    public short wGroup { get; set; }

    public short wLocalID { get; set; }

    public short wMapID { get; set; }

    public float fPosX { get; set; }

    public float fPosY { get; set; }

    public float fPosZ { get; set; }

    public short wDir { get; set; }

    public byte bCountry { get; set; }

    public byte bCount { get; set; }

    public byte bRange { get; set; }

    public byte bArea { get; set; }

    public byte bLink { get; set; }

    public byte bProb { get; set; }

    public byte bRoamType { get; set; }

    public int dwRegion { get; set; }

    public int dwDelay { get; set; }

    public byte bEvent { get; set; }

    public short wPartyID { get; set; }
}
