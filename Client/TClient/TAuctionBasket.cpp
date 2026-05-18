#include "stdafx.h"


//////////////////////////////////////////////////////////////////////////
//	Virtual Function Implementation.
CTAuctionBasket::CTAuctionBasket( TComponent* pParent, FRAMEDESC_SHAREDPTR pDESC )
:	CTAuctionInnerFrame( pParent, pDESC, TAUCTION_BASKET )
{
	m_FindInfo.Clear();
}

CTAuctionBasket::~CTAuctionBasket()
{

}

void	CTAuctionBasket::RequestInfo()
{
	CTAuctionInnerFrame::RequestInfo();

	CTAuctionCommander* pComm = CTAuctionCommander::GetInstance();
	pComm->RequestBasketList( GetFindInfo() );
}

void	CTAuctionBasket::RequestUpdatePage()
{
	CTAuctionInnerFrame::RequestUpdatePage();

	CTAuctionCommander::GetInstance()
		->RequestBasketList( GetFindInfo() );
}

void	CTAuctionBasket::ResetInfo()
{

}
//	End of Virtual Function.
//////////////////////////////////////////////////////////////////////////

