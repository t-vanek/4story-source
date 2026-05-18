#include "stdafx.h"
#include "TClientGame.h"
#include "TCharInfoNewInner.h"
#include "TCharInfoNewDlg.h"
#include "TCharInfoNewWarDlg.h"
#include "TCharInfoNewPVPDlg.h"

CTCharInfoNewDlg::CTCharInfoNewDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc, TCMLParser *pParser, CD3DDevice *pDevice, WORD wFrameID, CTClientChar* pHost)
:	CTFrameGroupBase( pParent, pDesc, 8032 )
{
	TButton *pClose = (TButton *)FindKid(1276);
	pClose->m_menu[TNM_LCLICK] = GM_CLOSE_UI;

	TComponent* m_pPOS = FindKid(8032);
	if(m_pPOS)
	{
		m_pPOS->GetComponentPos(&m_ptFRAMEPOS);
		RemoveKid(m_pPOS);
		delete m_pPOS;
	}

	static DWORD dwTabs[] = {
		26350,
		26351,
		26352,
		27592
	};

	for(BYTE i=0;i<4;++i)
		m_pTabs[i] = (TButton*) FindKid(dwTabs[i]);


	m_bTab = 0;
}

CTCharInfoNewDlg::~CTCharInfoNewDlg()
{

}

ITDetailInfoPtr	CTCharInfoNewDlg::GetTInfoKey( const CPoint& pt )
{
	ITDetailInfoPtr pointer;
	if(!m_bTab)
		return m_pFRAME->GetTInfoKey( pt );
	else
		return static_cast<CTClientUIBase*>(m_pFRAMES[m_bTab - 1])->GetTInfoKey(pt);
	
	return pointer;
}

void CTCharInfoNewDlg::ShowComponent( BOOL bVisible )
{
	m_bTab = 0;

	ResetTabs();
	CTFrameGroupBase::ShowComponent( bVisible );
}

void CTCharInfoNewDlg::ResetTabs()
{
	BYTE bTab = m_bTab;
	for(BYTE i=0;i<TAB_COUNT-1;++i)
		m_pFRAMES[i]->ShowComponent(FALSE);

	m_pFRAME->ShowComponent(FALSE);

	if(!bTab)
	{
		m_pFRAME->ShowComponent(TRUE);
		m_pFRAME->MoveComponent(m_ptFRAMEPOS);
	}
	else
	{
		m_pFRAMES[bTab - 1]->ShowComponent(TRUE);
		m_pFRAMES[bTab - 1]->MoveComponent(m_ptFRAMEPOS);
	}

	for(BYTE i=0;i<4;++i)
	{
		if(bTab == i)
			m_pTabs[i]->Select(TRUE);
		else
			m_pTabs[i]->Select(FALSE);
	}
}

void CTCharInfoNewDlg::OnLButtonDown(UINT nFlags, CPoint pt)
{
	if (!CTClientGame::IsInBOWMap() && !CTClientGame::IsInBRMap())
	{
		for (BYTE i = 0; i < 4;++i)
		{
			if (m_pTabs[i]->HitTest(pt))
			{
				if (TAB_TITLE == i)
				{
					if (CTClientGame::GetInstance()->GetSession())
						CTClientGame::GetInstance()->GetSession()->SendCS_TITLELIST_REQ();
				}

				m_bTab = i;
				ResetTabs();
				break;
			}
		}
	}

	CTFrameGroupBase::OnLButtonDown( nFlags, pt );
}

TDROPINFO CTCharInfoNewDlg::OnDrop( CPoint point)
{
	TDROPINFO vDrop;

	if(m_bTab == TAB_CHARACTER)
		vDrop = m_pFRAME->OnDrop(point);

	return vDrop;
}

BYTE CTCharInfoNewDlg::OnBeginDrag( LPTDRAG pDRAG, CPoint point)
{
	if(m_bTab == TAB_CHARACTER)
		return m_pFRAME->OnBeginDrag(pDRAG, point);

	return FALSE;
}

BOOL CTCharInfoNewDlg::DoMouseWheel( UINT nFlags, short zDelta, CPoint pt)
{
	if(m_bTab == TAB_TITLE)
		return m_pFRAMES[TAB_TITLE - 1]->DoMouseWheel(nFlags, zDelta, pt);

	return FALSE;
}

HRESULT CTCharInfoNewDlg::Render(DWORD dwTickCount)
{
	if(IsVisible())
	{
		ResetTabs();
		return CTFrameGroupBase::Render(dwTickCount);
	}

	return S_OK;
}

BOOL CTCharInfoNewDlg::SortByTime( LPTLATESTPVPINFO pLeft, LPTLATESTPVPINFO pRight )
{
	if(pLeft->m_dlDate > pRight->m_dlDate)
		return TRUE;
	else
		return FALSE;

	return FALSE;
}

BOOL CTCharInfoNewDlg::GetTChatInfo(const CPoint& point, TCHATINFO& outInfo )
{
	if(m_bTab == TAB_CHARACTER)
		return m_pFRAME->GetTChatInfo(point, outInfo );

	return FALSE;
}
