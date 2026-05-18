using System;
using System.Collections.Generic;

namespace FourStory.Persistence.Game;

public partial class TAUCTIONBIDDER
{
    public int dwAuctionID { get; set; }

    public int dwCharID { get; set; }

    public long dlBidPrice { get; set; }

    public DateTime DateBid { get; set; }
}
