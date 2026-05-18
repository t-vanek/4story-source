using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TAUCTIONTABLE
{
    public int dwAuctionID { get; set; }

    public short wNpcID { get; set; }

    public int dwCharID { get; set; }

    public DateTime DateStart { get; set; }

    public DateTime DateEnd { get; set; }

    public long dlDirectPrice { get; set; }

    public long dlStartPrice { get; set; }

    public long dlItemID { get; set; }

    public byte bBidCount { get; set; }
}
