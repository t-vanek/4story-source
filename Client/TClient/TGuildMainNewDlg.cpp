#include "stdafx.h"
#include "TClientGame.h"

CTGuildMainNew::CTGuildMainNew(TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc)
	: CTFrameGroupBase(pParent, pDesc, ID_CTRLINST_INNERPOS)
{
	TButton* m_pPrepare;
	TButton* m_pMove;
	TComponent* m_pSedyRam;

	m_pPrepare = (TButton*)FindKid(27710);
	m_pMove = (TButton*)FindKid(28024);
	m_pSedyRam = (TComponent*)FindKid(27704);

	RemoveKid(m_pPrepare);
	RemoveKid(m_pMove);
	RemoveKid(m_pSedyRam);
}

CTGuildMainNew::~CTGuildMainNew()
{

}

// ======================================================================
ITDetailInfoPtr CTGuildMainNew::GetTInfoKey(const CPoint& point)
{
	if (m_nSelectedFrame != T_INVALID)
	{
		FrameInfo& frminf = m_FrameInfoArray[m_nSelectedFrame];
		if (frminf.m_pFrameCtrl)
			return frminf.m_pFrameCtrl->GetTInfoKey(point);
	}

	ITDetailInfoPtr pInfo;
	return pInfo;
}

void CTGuildMainNew::OnKeyDown(UINT nChar, int nRepCnt, UINT nFlags)
{
	CTFrameGroupBase::OnKeyDown(nChar, nRepCnt, nFlags);
}

// ======================================================================
//void CTGuildMainNew::OnLButtonUp(UINT nFlags, CPoint pt)
//{
//	CTFrameGroupBase::OnLButtonUp(nFlags,pt);
//}
// ======================================================================
void CTGuildMainNew::OnLButtonDown(UINT nFlags, CPoint pt)
{
	/*ITInnerFrame* pInnerFrame = GetInnerFrame( m_nSelectedFrame );
	if( pInnerFrame &&
	pInnerFrame->IsVisible() )
	{
	CTFrameGroupBase::OnLButtonDown( nFlags, pt );
	}*/
	CTFrameGroupBase::OnLButtonDown(nFlags, pt);
}

TEdit*	CTGuildMainNew::GetCurEdit()
{
	if (!IsVisible())
		return NULL;

	ITInnerFrame* pInnerFrame = GetInnerFrame(m_nSelectedFrame);
	if (pInnerFrame &&
		pInnerFrame->IsVisible())
	{
		return static_cast< CTGuildSummaryNewDlg* >(pInnerFrame)->GetCurEdit();
	}

	return NULL;
}

// ======================================================================
void  CTGuildMainNew::ShowComponent(BOOL bVisible)
{
	CTFrameGroupBase::ShowComponent(bVisible);
}

//void CTGuildMainNew::ResetPosition()
//{
//	CTFrameGroupBase::ResetPosition();
//}

BYTE CTGuildMainNew::OnBeginDrag(LPTDRAG pDRAG, CPoint point)
{
	if (m_nSelectedFrame != T_INVALID)
	{
		FrameInfo& frminf = m_FrameInfoArray[m_nSelectedFrame];
		if (frminf.m_pFrameCtrl)
			return frminf.m_pFrameCtrl->OnBeginDrag(pDRAG, point);
	}

	return FALSE;
}