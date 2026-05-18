#include "stdafx.h"
#include "Resource.h"
#include "TClientGame.h"
#include "TClientWnd.h"
#include "TBoxOpenList.h"
#include "TBoxOpen.h"

#define MAX_ITEMS_IN_LIST      7

CTBoxOpenListDlg::CTBoxOpenListDlg( TComponent* pParent, FRAMEDESC_SHAREDPTR pDesc)
: CTClientUIBase( pParent, pDesc)
{
	m_pItemName[0] = FindKid( 7746 );
	m_pItemIcon[0] = (TImageList*) FindKid( 25609 );
	m_pHighlight[0] = FindKid( 25608 );
	m_pTSCROLL = (TScroll*)FindKid( 8512 );

	m_pItemIcon[0]->m_strText.Empty();

	for( BYTE i = 1; i < MAX_ITEMS_IN_LIST; ++i )
	{
		CPoint m_vBasisName, m_vBasisIcon, m_vBasisHL;
		m_pHighlight[i] = new TComponent(this, m_pHighlight[0]->m_pDESC);
		m_pItemName[i] = new TComponent(this, m_pItemName[0]->m_pDESC);
		m_pItemIcon[i] = new TImageList(this, m_pItemIcon[0]->m_pDESC);

		m_pHighlight[i]->m_id = GetUniqueID();
		m_pItemName[i]->m_id = GetUniqueID();
		m_pItemIcon[i]->m_id = GetUniqueID();

		m_pHighlight[0]->GetComponentPos(&m_vBasisHL);
		m_pItemName[0]->GetComponentPos(&m_vBasisName);
		m_pItemIcon[0]->GetComponentPos(&m_vBasisIcon);

		m_vBasisName.y += i*60; 
		m_vBasisIcon.y += i*60;
		m_vBasisHL.y += i*60;

		m_pItemName[i]->MoveComponent(m_vBasisName);
		m_pItemIcon[i]->MoveComponent(m_vBasisIcon);
		m_pHighlight[i]->MoveComponent(m_vBasisHL);

		AddKid(m_pHighlight[i]);
		AddKid(m_pItemName[i]);
		AddKid(m_pItemIcon[i]);
	}

	m_nListTop = 0;

	m_pTSCROLL->SetScrollType(TRUE);
}

CTBoxOpenListDlg::~CTBoxOpenListDlg()
{
	Odmeny.clear();
}

HRESULT CTBoxOpenListDlg::Render(DWORD dwTickCount)
{

	if( IsVisible() )
	{

		for( INT i=0 ; i < MAX_ITEMS_IN_LIST ; ++i )
		{

			if( m_nListTop+i >= Odmeny.size() )
			{
				m_pItemName[ i ]->m_strText.Empty();
				m_pItemIcon[ i ]->SetCurImage(0);
				m_pHighlight[ i ]->ShowComponent( FALSE );
			}
			else
			{
				if (!Odmeny[i + m_nListTop].m_pTITEM)
					return CTClientUIBase::Render(dwTickCount);

				LPTITEM pITEM = CTChart::FindTITEMTEMP( Odmeny[ i+m_nListTop ].m_pTITEM->GetItemID() );
				LPTITEMVISUAL pITEMV = NULL;

				if( pITEM )
					pITEMV = CTChart::FindTITEMVISUAL( pITEM->m_wVisual[0] );
				if( pITEMV )
				{
					m_pItemName[ i ]->m_strText = pITEM->m_strNAME;
					m_pItemIcon[ i ]->SetCurImage( pITEMV->m_wIcon );
				}

				m_pHighlight[ i ]->ShowComponent(TRUE);
			}
		}
	}

	return CTClientUIBase::Render( dwTickCount );
}

void CTBoxOpenListDlg::SetSession(WORD m_wBoxID, BYTE m_bInvenID, BYTE m_bSlotID)
{
	for (Reward& Odmena : Odmeny)
		if (Odmena.m_pTITEM)
			delete Odmena.m_pTITEM;
	Odmeny.clear();


	Release();
	m_nListTop = 0;
	UpdateScrollPosition();

	CTClientGame* pGAME = CTClientGame::GetInstance();
	pGAME->SwitchFocus(this);
	CTBoxOpenDlg* pDLG = static_cast<CTBoxOpenDlg*>( pGAME->GetFrame(TFRAME_OPENBOX) );

	CPoint pPoint;
	pDLG->GetComponentPos( &pPoint );

	pPoint.x += 405;
	MoveComponent( pPoint );

	LPTITEM pITEM = CTChart::FindTITEMTEMP( m_wBoxID );
	if( pITEM )
		pGAME->GetSession()->SendCS_GETREWARDS_REQ( pITEM->m_bKind, pITEM->m_wUseValue );
}

void CTBoxOpenListDlg::ResetPosition()
{
	CTClientGame* pGAME = CTClientGame::GetInstance();
	CTBoxOpenDlg* pDLG = static_cast<CTBoxOpenDlg*>( pGAME->GetFrame( TFRAME_OPENBOX ) );

	CPoint pPoint;
	pDLG->GetComponentPos( &pPoint );





	pPoint.x += 405;
	MoveComponent( pPoint );
}

void CTBoxOpenListDlg::AddItems(CTClientItem* pTITEM)
{
	Reward Odmena;
	Odmena.m_pTITEM = pTITEM;



	Odmeny.push_back(Odmena);
}

void CTBoxOpenListDlg::Release()
{



	m_nListTop = 0;
	UpdateScrollPosition();
}

ITDetailInfoPtr CTBoxOpenListDlg::GetTInfoKey( const CPoint& point )
{
	ITDetailInfoPtr pInfo;

	if(!IsVisible())
		return pInfo;



	for( BYTE i = 0; i < MAX_ITEMS_IN_LIST; ++i )
	{
		if( m_pItemIcon[ i ] && m_pItemIcon[ i ]->HitTest(point) ) 
		{
			if( m_nListTop >= 0 && (i + m_nListTop) < Odmeny.size()  )
			{
				if (!Odmeny[i + m_nListTop].m_pTITEM)

					return pInfo;







				CRect rc;
				GetComponentRect( &rc );

				pInfo = CTDetailInfoManager::NewItemInst(Odmeny[i + m_nListTop].m_pTITEM, rc );
				pInfo->SetCanHandleSecondInfo( TRUE );
			}
		}
	}

	return pInfo;
}

void CTBoxOpenListDlg::OnNotify(DWORD from, WORD msg, LPVOID param)
{
	switch(msg)
	{
	case TNM_LINEUP:
		{
			m_nListTop--;
			if( m_nListTop < 0 ) 
				m_nListTop = 0;
		}

		break;

	case TNM_LINEDOWN:
		if( MAX_ITEMS_IN_LIST < Odmeny.size()-m_nListTop )
		{
			m_nListTop++;

			if( m_nListTop > Odmeny.size() - MAX_ITEMS_IN_LIST ) 
				m_nListTop = Odmeny.size() - MAX_ITEMS_IN_LIST;
		}

		break;

	case TNM_VSCROLL:
		if( m_pTSCROLL && m_pTSCROLL->IsTypeOf( TCML_TYPE_SCROLL ) )
		{
			int nRange;
			int nPos;

			m_pTSCROLL->GetScrollPos( nRange, nPos );
			m_nListTop = nRange ? nPos : 0;
		}

		break;
	}


	CTClientUIBase::OnNotify( from, msg, param);
}

BOOL CTBoxOpenListDlg::DoMouseWheel( UINT nFlags, short zDelta, CPoint pt)
{
	BOOL bRESULT = FALSE;

	if(!CanProcess() || !m_pTSCROLL) 
		return FALSE;

	for( INT i=0 ; i < MAX_ITEMS_IN_LIST ; ++i )
		if( HitTest( pt ) )
		{
			bRESULT = TRUE;
			break;
		}

	if(!bRESULT && m_pTSCROLL->HitTest( pt ))
		bRESULT = TRUE;

	if(!bRESULT)
		return bRESULT;

	int nRange, nPos;
	m_pTSCROLL->GetScrollPos( nRange, nPos);

	m_nListTop += zDelta>0? -1 : 1;
	m_nListTop = min(max(m_nListTop, 0), nRange);

	UpdateScrollPosition();

	return TRUE;
}


void CTBoxOpenListDlg::UpdateScrollPosition()
{
	if( m_pTSCROLL &&
	    m_pTSCROLL->IsTypeOf(TCML_TYPE_SCROLL))
	{
		int nRange = Odmeny.size() - MAX_ITEMS_IN_LIST;
		if(nRange < 0)
			nRange = 0;

		m_pTSCROLL->SetScrollPos( nRange, m_nListTop);
	}
}