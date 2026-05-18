#include "stdafx.h"
#include "TTitleDlg.h"
#include "TClientGame.h"

CTTitleList::CTTitleList(CTTitleDlg *pParent, FRAMEDESC_SHAREDPTR pDesc)
: TList( pParent, pDesc ),
m_pParent( pParent )
{
}

void CTTitleList::OnNotify( DWORD from, WORD msg, LPVOID param )
{
	TList::OnNotify( from, msg, param );
	m_pParent->_UpdateCheckBox();
}

BOOL CTTitleList::DoMouseWheel( UINT nFlags, short zDelta, CPoint pt)
{
	BOOL bRet = TList::DoMouseWheel( nFlags, zDelta, pt );
	m_pParent->_UpdateCheckBox();
	return bRet;
}//

BOOL SortTitle(TITLE begin, TITLE end)
{
	if (begin.m_bGroupID < end.m_bGroupID)
		return TRUE;
	else if (begin.m_bGroupID > end.m_bGroupID)
		return FALSE;
	else if (begin.m_bGroupID == end.m_bGroupID)
	{
		if (begin.m_dwRequirement < end.m_dwRequirement)
			return TRUE;
		else
			return FALSE;
	}

	return TRUE;
}

CTTitleList::~CTTitleList()
{
}

static DWORD dwSlot[TITLESLOT_COUNT] =
{
	ID_CTRLINST_SLOT_TITLE1,
	ID_CTRLINST_SLOT_TITLE2,
	ID_CTRLINST_SLOT_TITLE3,
	ID_CTRLINST_SLOT_TITLE4,
	ID_CTRLINST_SLOT_TITLE5,
	ID_CTRLINST_SLOT_TITLE6,
	ID_CTRLINST_SLOT_TITLE7,
	ID_CTRLINST_SLOT_TITLE8,
	ID_CTRLINST_SLOT_TITLE9
};

CTTitleDlg::CTTitleDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
:	CTClientUIBase( pParent, pDesc )
{
	TList *pList = static_cast<TList*>(FindKid(ID_CTRLINST_TITLE_MAINLIST));
	RemoveKid( pList );

	m_pMainList = new CTTitleList( this, pList->m_pDESC.lock() );
	m_pMainList->m_id = pList->m_id;

	delete pList;
	AddKid( m_pMainList );

	m_pAqList = static_cast<TList*>( FindKid(ID_CTRLINST_TITLE_LEFTLIST) );
	m_pGroupList = static_cast<TList*>( FindKid(ID_CTRLINST_TITLE_GROUPLIST) );
	m_pBtn = static_cast<TButton *>(FindKid(ID_CTRLINST_TITLE_GROUPLISTBTN));
	m_pDescription = FindKid(ID_CTRLINST_TITLE_DESCRIPTION);
	m_pAqTitle = FindKid(ID_CTRLINST_TITLE_AQ_TITLE);

	RemoveKid(m_pGroupList);
	AddKid(m_pGroupList);

	m_bListVisible = TRUE;

	TComponent *pBase = FindKid(dwSlot[0]);

	pBase->GetComponentPos( &m_ptBasisPos );
	CPoint ptTemp = CPoint( 0, 0 );

	m_pCheckBox = dynamic_cast<TButton*>(FindKid(ID_CTRLINST_CHECKBOX));
	m_pCheckBox->GetComponentPos( &ptTemp );
	m_vCheckBox_Offset = ptTemp - m_ptBasisPos;

	RemoveKid(m_pCheckBox);

	DWORD bStyle = GetStyle();
	SetStyle(bStyle |+ TS_CUSTOM_COLOR);
	m_dwColor = 0xD0FFFFFF;
}

CTTitleDlg::~CTTitleDlg()
{	
	delete m_pCheckBox;
}

void CTTitleDlg::Add(
	DWORD dwTitleID,
	const CString& strTitle,
	const CString& strRequirement,
	BYTE bGroupID,
	DWORD dwRequirement,
	BYTE bCount)
{
	TITLE vTitle;

	vTitle.m_dwTitleID = dwTitleID;
	vTitle.m_strTitle = strTitle;
	vTitle.m_strRequirement = strRequirement;
	vTitle.m_bGroupID = bGroupID;
	vTitle.m_dwRequirement = dwRequirement;

	m_vTitle.push_back( vTitle );

	if (m_vTitle.size() == bCount)
	{
		std::sort(m_vTitle.begin(), m_vTitle.end(), SortTitle);
		for (BYTE t = 0; t < m_vTitle.size(); ++t)
		{
			int nLine = m_pMainList->AddString(m_vTitle[t].m_strTitle, 1);

			m_pMainList->SetItemString(nLine, 2, m_vTitle[t].m_strRequirement);
			m_pMainList->SetItemData(nLine, 0, m_vTitle[t].m_dwTitleID);
			m_pMainList->SetItemData(nLine, 1, m_vTitle[t].m_dwTitleID);
			m_pMainList->SetItemData(nLine, 2, m_vTitle[t].m_dwTitleID);

			for(int i = 0; i < m_vTitleAV.size(); i++)
			{
				if(m_vTitleAV[i].m_dwTitleID == m_vTitle[t].m_dwTitleID)
				{
					int nLine = m_pAqList->AddString(m_vTitle[t].m_strTitle);
					m_pAqList->SetItemData(nLine, 0, m_vTitle[t].m_dwTitleID);

					if(m_vTitleAV[i].m_bChecked)
					{
						m_pAqList->SetCurSelItem(m_pAqList->GetItemCount() - 1);
						m_pAqTitle->m_strText = m_vTitle[t].m_strTitle;
					}
				}
			}

			_UpdateCheckBox();
		}
	}
}

void CTTitleDlg::AddAV(
	DWORD dwTitleID,
	const CString& strTitle,
	const CString& strRequirement,
	BYTE bGroupID,
	DWORD dwRequirement,
	BYTE bCount)
{
	TITLE vTitle;

	vTitle.m_dwTitleID = dwTitleID;
	vTitle.m_strTitle = strTitle;
	vTitle.m_strRequirement = strRequirement;
	vTitle.m_bGroupID = bGroupID;
	vTitle.m_dwRequirement = dwRequirement;

	m_vTitle.push_back( vTitle );

	if (m_vTitle.size() == bCount)
	{
		std::sort(m_vTitle.begin(), m_vTitle.end(), SortTitle);
		for (int i = 0; i < m_vTitle.size(); ++i)
		{
			int nLine = m_pMainList->AddString(m_vTitle[i].m_strTitle, 1);

			m_pMainList->SetItemString(nLine, 2, m_vTitle[i].m_strRequirement);
			m_pMainList->SetItemData(nLine, 0, m_vTitle[i].m_dwTitleID);
			m_pMainList->SetItemData(nLine, 1, m_vTitle[i].m_dwTitleID);
			m_pMainList->SetItemData(nLine, 2, m_vTitle[i].m_dwTitleID);

			_UpdateCheckBox();
		}
	}
}

void CTTitleDlg::SetTitle(DWORD dwTitle, BYTE bAvailable, BYTE bSelected)
{
	TITLEAV Title;

	Title.m_dwTitleID = dwTitle;
	Title.m_bAvailable = bAvailable;
	Title.m_bChecked = bSelected;

	m_vTitleAV.push_back(Title);
}

void CTTitleDlg::UpdateTitle(WORD wTitleID)
{
	for(int i = 0; i <  m_vTitleAV.size(); i++ )
	{
		if(m_vTitleAV[i].m_dwTitleID == wTitleID)
		{
			for(int j = 0; j < m_vTitleAV.size(); j++)
				m_vTitleAV[j].m_bChecked = FALSE;

			m_vTitleAV[i].m_bChecked = TRUE;

			LPTTITLE pTitle = CTChart::FindTTITLETEMP(wTitleID);

			if(pTitle)
				m_pAqTitle->m_strText = pTitle->m_strTitle;

			int nAqLine = 0;

			for(; nAqLine < m_pAqList->GetItemCount(); nAqLine++)
			{
				if((*(m_pAqList->m_items[nAqLine]))[0]->m_param == wTitleID)
					break;
			}

			m_pAqList->SetCurSelItem(nAqLine);
		}
	}

	_UpdateCheckBox();
}

void CTTitleDlg::_UpdateCheckBox()
{
	int nTop = m_pMainList->GetTop();

	for( INT i=0 ; i < m_pMainList->GetItemPerPage() ; ++i )
	{
		BOOL bValid = ( nTop+i < m_pMainList->GetItemCount() );

		if( bValid )
		{
			if( i < m_vCheckBox.size() )
			{
				BYTE bChecked = FALSE;
				BYTE bShow = FALSE;

				for(int j = 0; j < m_vTitleAV.size(); j++)
				{
					if(m_vTitleAV[j].m_dwTitleID == m_vTitle[ nTop+i ].m_dwTitleID && m_vTitleAV[j].m_bAvailable)
					{
						bShow = TRUE;

						if(m_vTitleAV[j].m_bChecked)
							bChecked = TRUE;
					}
				}

				m_vCheckBox[ i ].m_nIndex = nTop + i;

				m_vCheckBox[ i ].m_pCheckBtn->Select(bChecked);
				m_vCheckBox[ i ].m_pCheckBtn->ShowComponent(bShow);
			}
			else
			{
				TComponent *pBase = FindKid(dwSlot[i]);

				CHECKBOX vOP;
				vOP.m_pCheckBtn = new TButton( this, *m_pCheckBox );

				vOP.m_pCheckBtn->m_id = GetUniqueID();

				static_cast<TButton*>( vOP.m_pCheckBtn )->SetStateButton();

				pBase->GetComponentPos( &m_ptBasisPos );
				vOP.m_pCheckBtn->MoveComponent( m_ptBasisPos + m_vCheckBox_Offset );
				m_vCheckBox.push_back( vOP );

				m_vCheckBox[ i ].m_nIndex = nTop + i;

				AddKid( m_vCheckBox[ i ].m_pCheckBtn );
				m_vCheckBox[ i ].m_pCheckBtn->EnableComponent(FALSE);
			}
		}
		else
		{
			if( i < m_vCheckBox.size() )
			{
				m_vCheckBox[ i ].m_pCheckBtn->ShowComponent(FALSE);
				m_vCheckBox[ i ].m_nIndex = -1;
			}
		}
	}
}

void CTTitleDlg::OnLButtonDown( UINT nFlags, CPoint pt )
{		
	if(!m_pGroupList->HitTest(pt) && !m_pBtn->HitTest(pt))
		m_bListVisible = FALSE;

	m_pBtn->SetPushButton();

	if( m_pBtn->HitTest(pt) )
		m_bListVisible = !m_bListVisible;

	if(m_pGroupList->HitTest(pt))
	{
		TListItem* pHITITEM = m_pGroupList->GetHitItem( pt );

		if( pHITITEM && pHITITEM != NULL)
		{
			MAPTTITLEGROUPTEMP::iterator itTTITLEGROUP;
			MAPTTITLETEMP::iterator itTTITLE;

			ClearMainList();

			DWORD dwID = m_pGroupList->GetHitItemData(pt);
			BYTE bCount = 0;

			for( itTTITLE = CTChart::m_mapTTITLETEMP.begin(); itTTITLE != CTChart::m_mapTTITLETEMP.end(); itTTITLE++)
			{
				if((*itTTITLE).second->m_bKind == dwID || dwID == NULL)
					bCount++;
			}

			for (itTTITLE = CTChart::m_mapTTITLETEMP.begin(); itTTITLE != CTChart::m_mapTTITLETEMP.end(); ++itTTITLE)
			{
				if ((*itTTITLE).second->m_bKind == dwID || dwID == NULL)
				{
					AddAV((*itTTITLE).second->m_wTitleID, (*itTTITLE).second->m_strTitle, (*itTTITLE).second->m_strRequirement, (*itTTITLE).second->m_bKind, (*itTTITLE).second->m_dwRequirement, bCount);
							
					LPTTITLEGROUP pTitleGroup = CTChart::FindTTITLEGROUPTEMP(BYTE(dwID));
					m_pBtn->m_strText = pTitleGroup->m_strCaption;
				}
			}
			m_bListVisible = FALSE;
		}
	}

	if(m_pMainList->HitTest(pt))
	{
		TListItem* pHITITEM = m_pMainList->GetHitItem( pt );

		if( pHITITEM && pHITITEM != NULL)
		{
			DWORD dwTitleID;
	
			dwTitleID = m_pMainList->GetHitItemData( pt );

			LPTTITLE pTitle = CTChart::FindTTITLETEMP(dwTitleID);

			if(pTitle)
				m_pDescription->m_strText = pTitle->m_strDescription;

			for(int i = 0; i < m_vTitleAV.size(); i++)
			{
				if(m_vTitleAV[i].m_dwTitleID == dwTitleID && !m_vTitleAV[i].m_bChecked)
				{
					CTClientSession* pSESSION = CTClientGame::GetInstance()->GetSession();

					if(pSESSION)
						pSESSION->SendCS_CHANGETITLE_REQ(LOWORD(dwTitleID));
				}
			}
		}
	}

	CTClientUIBase::OnLButtonDown( nFlags, pt);

	if(m_pAqList->HitTest(pt))
	{
		TListItem* pHITITEM = m_pAqList->GetHitItem( pt );

		if( pHITITEM && pHITITEM != NULL)
		{
			DWORD dwTitleID = m_pAqList->GetItemData(m_pAqList->GetSel(), 0);

			CTClientSession* pSESSION = CTClientGame::GetInstance()->GetSession();

			if(pSESSION)
				pSESSION->SendCS_CHANGETITLE_REQ(LOWORD(dwTitleID));
		}
	}
}

void CTTitleDlg::SwitchFocus(TComponent *pCandidate)
{
	if (m_pFocus == pCandidate)
		return;

	if (m_pFocus)
	{
		m_pFocus->SetFocus(FALSE);
		m_pFocus->OnLoseFocus(pCandidate);
	}

	if (pCandidate)
	{
		pCandidate->SetFocus(TRUE);
		pCandidate->OnSetFocus(m_pFocus);
	}

	m_pFocus = pCandidate;
}

void CTTitleDlg::AddLists()
{
	m_bListVisible = FALSE;

	MAPTTITLETEMP::iterator itTTITLE;
	MAPTTITLEGROUPTEMP::iterator itTTITLEGROUP;

	for( itTTITLE = CTChart::m_mapTTITLETEMP.begin(); itTTITLE != CTChart::m_mapTTITLETEMP.end(); itTTITLE++)
	{
		Add((*itTTITLE).second->m_wTitleID, (*itTTITLE).second->m_strTitle, (*itTTITLE).second->m_strRequirement, (*itTTITLE).second->m_bKind, (*itTTITLE).second->m_dwRequirement, CTChart::m_mapTTITLETEMP.size());
	}

	for( itTTITLEGROUP = CTChart::m_mapTTITLEGROUPTEMP.begin(); itTTITLEGROUP != CTChart::m_mapTTITLEGROUPTEMP.end(); itTTITLEGROUP++)
	{
		if((*itTTITLEGROUP).second->m_bID == 0)
			m_pBtn->m_strText = (*itTTITLEGROUP).second->m_strCaption;

		int nLine = m_pGroupList->AddString((*itTTITLEGROUP).second->m_strCaption);
		m_pGroupList->SetItemData(nLine, 0, (*itTTITLEGROUP).second->m_bID);
	}
}

void CTTitleDlg::ClearLists()
{
	m_pMainList->RemoveAll();
	m_vTitle.clear();
	m_vTitleAV.clear();
	m_pAqList->RemoveAll();
	m_pGroupList->RemoveAll();
	m_pDescription->m_strText.Empty();
}

void CTTitleDlg::ClearMainList()
{
	m_pMainList->RemoveAll();
	m_vTitle.clear();

	for(int i = 0; i < m_vCheckBox.size(); i++)
		m_vCheckBox[i].m_pCheckBtn->ShowComponent(FALSE);
}

void CTTitleDlg::ClearAqList()
{
	m_vTitleAV.clear();
}

BYTE CTTitleDlg::FindTitleID(WORD wTitleID)
{
	for(int i = 0; m_vTitleAV.size(); i++)
	{
		if(m_vTitleAV[i].m_dwTitleID == wTitleID)
			return TRUE;
	}

	return FALSE;
}

HRESULT CTTitleDlg::Render(DWORD dwTickCount)
{
	m_pGroupList->ShowComponent(m_bListVisible);
	_UpdateCheckBox();

	return CTClientUIBase::Render(dwTickCount);
}