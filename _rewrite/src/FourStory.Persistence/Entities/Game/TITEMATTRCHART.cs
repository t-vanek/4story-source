using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TITEMATTRCHART
{
    public short wID { get; set; }

    public byte bKind { get; set; }

    public byte bGrade { get; set; }

    public short wMinAP { get; set; }

    public short wMaxAP { get; set; }

    public short wDP { get; set; }

    public short wMinMAP { get; set; }

    public short wMaxMAP { get; set; }

    public short wMDP { get; set; }

    public byte bBlockProb { get; set; }
}
