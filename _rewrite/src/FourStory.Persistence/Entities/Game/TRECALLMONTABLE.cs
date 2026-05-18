using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TRECALLMONTABLE
{
    public int dwOwnerID { get; set; }

    public int dwID { get; set; }

    public short wMonID { get; set; }

    public short wPetID { get; set; }

    public int dwATTR { get; set; }

    public byte bLevel { get; set; }

    public int dwHP { get; set; }

    public int dwMP { get; set; }

    public byte bSkillLevel { get; set; }

    public short wPosX { get; set; }

    public short wPosY { get; set; }

    public short wPosZ { get; set; }

    public int dwTime { get; set; }

    public byte bEffect { get; set; }
}
