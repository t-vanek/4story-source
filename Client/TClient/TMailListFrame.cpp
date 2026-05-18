#include "Stdafx.h"
#include "TMailListFrame.h"
#include "Resource.h"
#include "TClientGame.h"
#include "TMailSendDlg.h"
#include "TMailRecvDlg.h"

#define READ_MAIL_COLOR			D3DCOLOR_ARGB(255, 120,120,120)
#define NEW_MAIL_COLOR			D3DCOLOR_ARGB(255, 225,225,225)

// =======================================================
CTMailListFrame::CTMailListFrame(TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc)
	: CTClientUIBase(pParent,pDesc), m_nPrvScrPos(0), 
	  m_bNeedUpdate(TRUE), m_nSelectIdx(T_INVALID)
{

	CPoint ptPos;
	TComponent* pPos = FindKid(ID_CTRLINST_FRAME );
	pPos->GetComponentPos(&ptPos);
	m_pFRAME = new CTClientUIBase( this, CTClientGame::GetInstance()->m_pTParser->FindFrameTemplate(ID_FRAME_MAILBOX_BASE));
	AddKid(m_pFRAME);
	m_pFRAME->MoveComponent(ptPos);
	TComponent * pWar = FindKid(26106);
	TButton * pGet =  (TButton*)FindKid(26777);

	m_pMailDlgTitle = FindKid( 26105 );

	RemoveKid(pWar);
	RemoveKid(pGet);
	delete pWar;
	delete pGet;
	static const DWORD dwITEM[] = 
	{
		ID_CTRLINST_ITEM1,
		ID_CTRLINST_ITEM2,
		ID_CTRLINST_ITEM3,
		ID_CTRLINST_ITEM4,
		ID_CTRLINST_ITEM5,
		ID_CTRLINST_ITEM6
	};

	CTCLSlotArray vSlots;

	for(INT i=0; i<MAX_LINE; ++i)
	{

		CTMailSlot* pSlot = new CTMailSlot();	
		TComponent* Slot = static_cast<TComponent*>(m_pFRAME->FindKid(dwITEM[i]));		
		pSlot->SetSlot(i, Slot);
		vSlots.push_back(pSlot);
	}

	CTMailSlot* pSlot0 = static_cast<CTMailSlot*>( vSlots[0]);
	CPoint ptBase, ptTemp;
	pSlot0->m_pBase->GetComponentPos(&ptBase);
	
	
	pSlot0->m_pButton = new TButton(this, pSlot0->m_pBase->m_pDESC);
	pSlot0->m_pButton->m_id = GetUniqueID();

	pSlot0->m_pMailImg = m_pFRAME->FindKid(26818);
	pSlot0->m_pMailImg->GetComponentPos(&ptTemp);
	pSlot0->m_pMailImg->m_bNoHIT = TRUE;
	CPoint ptMailOffset = ptTemp - ptBase;

	pSlot0->m_pPackage = m_pFRAME->FindKid(26817);
	pSlot0->m_pPackage->GetComponentPos(&ptTemp);
	pSlot0->m_pPackage->m_bNoHIT = TRUE;
	CPoint ptPackOffset = ptTemp - ptBase;

	pSlot0->m_pSenderTxt = m_pFRAME->FindKid(ID_CTRLINST_TXT_TYPE);
	pSlot0->m_pSenderTxt->GetComponentPos(&ptTemp);
	pSlot0->m_pSenderTxt->m_bNoHIT = TRUE;
	CPoint ptSenderOffset = ptTemp - ptBase;

	pSlot0->m_pTitleTxt = m_pFRAME->FindKid(ID_CTRLINST_TXT_TITLE);
	pSlot0->m_pTitleTxt->GetComponentPos(&ptTemp);
	pSlot0->m_pTitleTxt->m_bNoHIT = TRUE;
	CPoint ptTitleOffset = ptTemp - ptBase;

	pSlot0->m_pTimeTxt = m_pFRAME->FindKid(ID_CTRLINST_TXT_DATA);
	pSlot0->m_pTimeTxt->GetComponentPos(&ptTemp);
	pSlot0->m_pTimeTxt->m_bNoHIT = TRUE;
	CPoint ptTimeOffset = ptTemp - ptBase;

	pSlot0->m_pCheck = (TButton*)m_pFRAME->FindKid(ID_CTRLINST_CHECKBOX);
	pSlot0->m_pCheck->GetComponentPos(&ptTemp);
	pSlot0->m_pCheck->SetStyle(TBS_STATE_BUTTON);
	CPoint ptCheckOffset = ptTemp - ptBase;
		
	pSlot0->m_pButton->m_id = GetUniqueID();
	pSlot0->m_pMailImg->m_id = GetUniqueID();
	pSlot0->m_pPackage->m_id = GetUniqueID();
	pSlot0->m_pSenderTxt->m_id = GetUniqueID();
	pSlot0->m_pTitleTxt->m_id = GetUniqueID();
	pSlot0->m_pTimeTxt->m_id = GetUniqueID();
	pSlot0->m_pCheck->m_id = GetUniqueID();

	m_pFRAME->RemoveKid( pSlot0->m_pButton );
	m_pFRAME->RemoveKid( pSlot0->m_pMailImg );
	m_pFRAME->RemoveKid( pSlot0->m_pPackage );
	m_pFRAME->RemoveKid( pSlot0->m_pSenderTxt );
	m_pFRAME->RemoveKid( pSlot0->m_pTitleTxt );
	m_pFRAME->RemoveKid( pSlot0->m_pTimeTxt );
	m_pFRAME->RemoveKid( pSlot0->m_pCheck );

	m_pFRAME->AddKid( pSlot0->m_pButton );
	m_pFRAME->AddKid( pSlot0->m_pMailImg );
	m_pFRAME->AddKid( pSlot0->m_pPackage );
	m_pFRAME->AddKid( pSlot0->m_pSenderTxt );
	m_pFRAME->AddKid( pSlot0->m_pTitleTxt );
	m_pFRAME->AddKid( pSlot0->m_pTimeTxt );
	m_pFRAME->AddKid( pSlot0->m_pCheck );

	pSlot0->m_pButton->MoveComponent(ptBase);
	pSlot0->m_pMailImg->MoveComponent(ptBase+ptMailOffset);
	pSlot0->m_pPackage->MoveComponent(ptBase+ptPackOffset);
	pSlot0->m_pSenderTxt->MoveComponent(ptBase+ptSenderOffset);
	pSlot0->m_pTitleTxt->MoveComponent(ptBase+ptTitleOffset);
	pSlot0->m_pTimeTxt->MoveComponent(ptBase+ptTimeOffset);
	pSlot0->m_pCheck->MoveComponent(ptBase+ptCheckOffset);

	pSlot0->m_pButton->SetStateButton();
	pSlot0->m_pButton->m_menu[TNM_DBLCLICK] = GM_SHOW_RECVMAIL;	

	for(INT i=1; i<MAX_LINE; ++i)
	{		
		CTMailSlot* pSlot = static_cast<CTMailSlot*>( vSlots[i]);
		pSlot->m_pButton = new TButton(this, pSlot->m_pBase->m_pDESC);
		pSlot->m_pMailImg = new TComponent(this, pSlot0->m_pMailImg->m_pDESC);
		pSlot->m_pPackage = new TComponent(this, pSlot0->m_pPackage->m_pDESC);
		pSlot->m_pSenderTxt = new TComponent(this, pSlot0->m_pSenderTxt->m_pDESC);
		pSlot->m_pTitleTxt = new TComponent(this, pSlot0->m_pTitleTxt->m_pDESC);
		pSlot->m_pTimeTxt = new TComponent(this, pSlot0->m_pTimeTxt->m_pDESC);
		pSlot->m_pCheck = new TButton(this, pSlot0->m_pCheck->m_pDESC);
		
		pSlot->m_pButton->m_id = GetUniqueID();
		pSlot->m_pMailImg->m_id = GetUniqueID();
		pSlot->m_pPackage->m_id = GetUniqueID();
		pSlot->m_pSenderTxt->m_id = GetUniqueID();
		pSlot->m_pTitleTxt->m_id = GetUniqueID();
		pSlot->m_pTimeTxt->m_id = GetUniqueID();
		pSlot->m_pCheck->m_id = GetUniqueID();


		pSlot->m_pMailImg->m_bNoHIT = TRUE;
		pSlot->m_pPackage->m_bNoHIT = TRUE;
		pSlot->m_pSenderTxt->m_bNoHIT = TRUE;
		pSlot->m_pTitleTxt->m_bNoHIT = TRUE;
		pSlot->m_pTimeTxt->m_bNoHIT = TRUE;
	
		pSlot->m_pButton->SetStateButton();
		pSlot->m_pButton->m_menu[TNM_DBLCLICK] = GM_SHOW_RECVMAIL;		

		m_pFRAME->AddKid( pSlot->m_pButton );
		m_pFRAME->AddKid( pSlot->m_pMailImg );
		m_pFRAME->AddKid( pSlot->m_pPackage );
		m_pFRAME->AddKid( pSlot->m_pSenderTxt );
		m_pFRAME->AddKid( pSlot->m_pTitleTxt );
		m_pFRAME->AddKid( pSlot->m_pTimeTxt );
		m_pFRAME->AddKid( pSlot->m_pCheck );
		pSlot->m_pCheck->SetStyle(TBS_STATE_BUTTON);
		
		pSlot->m_pBase->GetComponentPos(&ptBase);
		pSlot->m_pButton->MoveComponent(ptBase);
		pSlot->m_pMailImg->MoveComponent(ptBase+ptMailOffset);
		pSlot->m_pPackage->MoveComponent(ptBase+ptPackOffset);
		pSlot->m_pSenderTxt->MoveComponent(ptBase+ptSenderOffset);
		pSlot->m_pTitleTxt->MoveComponent(ptBase+ptTitleOffset);
		pSlot->m_pTimeTxt->MoveComponent(ptBase+ptTimeOffset);
		pSlot->m_pCheck->MoveComponent(ptBase+ptCheckOffset);
	}

	m_pList = new CTCtrlList();
	m_pList->SetSlot(&vSlots);
	m_pList->SetupScroll( static_cast<TScroll*>( m_pFRAME->FindKid( ID_CTRLINST_SCROLL )), 1);

	m_pScroll = NULL;
	
	m_pSendBtn = static_cast<TButton*>(FindKid(26780));
	m_pSendBtn->m_menu[TNM_LCLICK] = GM_SHOW_NEWMAIL;

	TComponent* pBTN = FindKid(26781);
	if( pBTN )
		pBTN->m_bWordBreak = FALSE;


	m_pCheckAll =  (TButton*)FindKid(26778);
	m_pCheckAll->SetStyle(TBS_STATE_BUTTON);
	
	m_wTotalCount = 0;
	ClearMail();
}
// -------------------------------------------------------
CTMailListFrame::~CTMailListFrame()
{
	delete m_pList;
}
// =======================================================

// =======================================================
void CTMailListFrame::AddMail(LPTMAIL_SIMPLE pMail)
{
	CTMailItem* Item = new CTMailItem();
	Item->pSimple = pMail;
	Item->pInfo = NULL;
	m_pList->AddItem(Item);
	NotifyUpdate();

	TButton *pDel = (TButton*) FindKid(26781);
	if(pDel)
	{
		pDel->EnableComponent(TRUE);
		pDel->m_pFont->m_dwColor = 0xFFebdcb4;
	}
}
// -------------------------------------------------------
void CTMailListFrame::SetMail(INT nIdx, LPTMAIL pMail)
{
	CTMailItem * pItem = static_cast<CTMailItem*>(m_pList->GetItem(nIdx));
	if(pItem)
		pItem->pInfo=pMail;
	
}
// =======================================================
bool less_mail_by_time(const CTMailItem& l, const CTMailItem& r)
{
	return (l.pSimple->m_nTime > r.pSimple->m_nTime);
}
void CTMailListFrame::SortMail()
{
	//std::sort(m_vMails.begin(), m_vMails.end(), less_mail_by_time);
	
	NotifyUpdate();
}
// =======================================================
void CTMailListFrame::RemoveMail(INT nIdx)
{
	m_pList->RemoveItem(nIdx);
	SetSelectedIndex(-1);
}
// -------------------------------------------------------
void CTMailListFrame::ClearMail()
{
	m_pList->ClearItem();
	SetSelectedIndex(-1);

	TButton *pDel = (TButton*) FindKid(26781);
	if(pDel)
	{
		pDel->EnableComponent(FALSE);
		pDel->m_pFont->m_dwColor = 0xFF696969;
	}
}
// =======================================================
LPTMAIL CTMailListFrame::GetMail(INT nIdx) const
{
	CTMailItem * pItem = static_cast<CTMailItem*>(m_pList->GetItem(nIdx));
	if(pItem)
		return pItem->pInfo;
	return NULL;
}
// -------------------------------------------------------
LPTMAIL_SIMPLE CTMailListFrame::GetMailSimple(INT nIdx) const
{
	CTMailItem * pItem = static_cast<CTMailItem*>(m_pList->GetItem(nIdx));
	if(pItem)
		return pItem->pSimple;
	return NULL;
}
// =======================================================
void CTMailListFrame::SetSelectedIndex(INT nIdx)
{
	m_nSelectIdx = nIdx;
}
// -------------------------------------------------------
INT CTMailListFrame::GetSelectedIndex() const
{
	return m_nSelectIdx;
}
// -------------------------------------------------------
INT CTMailListFrame::FindIndexByPostID(DWORD dwPostID) const
{
	for(int i=0;i<m_pList->GetItemCnt();i++)
	{
		CTMailItem * pItem = static_cast<CTMailItem*>(m_pList->GetItem(i));
		if(pItem && pItem->pSimple->m_dwPostID == dwPostID )
			return i;
	}
	return T_INVALID;
}
// =======================================================
BOOL CTMailListFrame::IsEmpty() const
{
	return FALSE;
}
// -------------------------------------------------------
UINT CTMailListFrame::GetCount() const
{
	return (UINT)m_pList->GetItemCnt();
}
// =======================================================
void CTMailListFrame::ViewMail(INT nIdx)
{

	
	CTMailItem * pItem = static_cast<CTMailItem*>(m_pList->GetItem(nIdx));
	if(!pItem)
		return;


	CTClientGame* pGame = CTClientGame::GetInstance();

	if( pItem->pInfo && pItem->pInfo->m_bRead )
	{
		CTMailRecvDlg* pDlg = static_cast<CTMailRecvDlg*>(
			pGame->GetFrame(TFRAME_MAIL_RECV));

		pDlg->SetInfo(pItem->pInfo, pItem->pSimple);
		pGame->EnableUI(TFRAME_MAIL_RECV);
	}
	else
	{
		CTClientSession* pSession = pGame->GetSession();
		if( !pSession )
			return;

		if(pItem->pSimple)
			pSession->SendCS_POSTVIEW_REQ( pItem->pSimple->m_dwPostID );
	}
}
// =======================================================
BOOL CTMailListFrame::IsNewMail() const
{
	for(int i=0;i<m_pList->GetItemCnt();i++)
	{
		CTMailItem * pItem = static_cast<CTMailItem*>(m_pList->GetItem(i));
		if(pItem && pItem->pSimple->m_bRead )
			return TRUE;
	}

	return FALSE;
}
// =======================================================
void CTMailListFrame::NotifyUpdate()
{
	m_bNeedUpdate = TRUE;
}
// -------------------------------------------------------
void CTMailListFrame::Update()
{
	//if( !m_bNeedUpdate )
	//	return;

    SortMail();
	INT nMailCnt = m_pList->GetItemCnt();
	
	for(INT i=0; i<MAX_LINE; ++i)
	{
		CTMailSlot * pSlot = static_cast<CTMailSlot*>(m_pList->GetSlot(i));
		INT nIndex = i+m_pList->GetScrollPos();

		pSlot->m_pButton->Select( nIndex == m_nSelectIdx );
		//pSlot->m_pCheck->Select(FALSE);
		
	}
	for(int i =0;i<m_pList->GetItemCnt();i++)
	{
		INT nIndex = i-m_pList->GetScrollPos();
		if(nIndex>=0 && nIndex<6)
		{

		CTMailSlot * pSlot = static_cast<CTMailSlot*>(m_pList->GetSlot(nIndex));
		CTMailItem * pItem = static_cast<CTMailItem*>(m_pList->GetItem(i));
		pSlot->m_pCheck->Select(pItem->SelectToDel);		
		pSlot->m_pCheck->EnableComponent(TRUE);
		}		
	}
	m_bNeedUpdate = FALSE;
}
// =======================================================
BOOL CTMailListFrame::DoMouseWheel( UINT nFlags, short zDelta, CPoint pt)
{
	if( !IsVisible() || !CanProcess() || !HitTest(pt) )
		return FALSE;

	return m_pList->DoMouseWheel(nFlags,zDelta,pt);

}
// =======================================================
void CTMailListFrame::ShowComponent(BOOL bVisible)
{
	CTClientUIBase::ShowComponent(bVisible);

	if( bVisible )
		NotifyUpdate();
}
// -------------------------------------------------------
HRESULT CTMailListFrame::Render(DWORD dwTickCount)
{	
	m_pList->Update();
	Update();
	return CTClientUIBase::Render(dwTickCount);
}
// =======================================================
void CTMailListFrame::OnLButtonDown(UINT nFlags, CPoint pt)
{
	for(INT i=0; i<MAX_LINE; ++i)
	{
		CTMailSlot * pSlot = static_cast<CTMailSlot*>(m_pList->GetSlot(i));
		TButton* pFrame = pSlot->m_pButton;
		if( pFrame->IsVisible() && pFrame->HitTest(pt) )
		{
			CTTextLinker *pTextLinker = CTTextLinker::GetInstance();
			CTClientGame *pTGAME = CTClientGame::GetInstance();
			DWORD dwInfo;

			if( pSlot->m_pTitleTxt->HitTestTextSetting(pt, &dwInfo) )
			{
				CTTextLinkData *pLinkData = pTextLinker->GetTextLinkData(dwInfo);

				if( pLinkData && pLinkData->IsItemType() )
				{
					CTClientItem *pItem = pLinkData->GetLastUnkpackItem();

					if(pItem)
						pTGAME->ShowChatDetailInfo(pItem);
				}
			}

			INT nSelIdx = i+m_pList->GetScrollPos();
			SetSelectedIndex(nSelIdx);
			break;
		}
			
	}


	for(int i =0;i<m_pList->GetItemCnt();i++)
	{
		INT nIndex = i-m_pList->GetScrollPos();
		if(nIndex>=0 && nIndex<6)
		{
				CTMailSlot * pSlot = static_cast<CTMailSlot*>(m_pList->GetSlot(nIndex));
				CTMailItem * pItem = static_cast<CTMailItem*>(m_pList->GetItem(i));
				if(pSlot->m_pCheck->HitTest(pt)){
				//pItem->SelectToDel = FALSE;
				if(pItem->SelectToDel == TRUE)
						pItem->SelectToDel = FALSE;
				else if(pItem->pSimple->m_bRead && pItem->pSimple->m_bContain ==0)
					    pItem->SelectToDel = TRUE;

				break;
				}
		}
	}

	

	CTClientUIBase::OnLButtonDown(nFlags,pt);
}
// -------------------------------------------------------
void CTMailListFrame::OnLButtonUp(UINT nFlags, CPoint pt)
{
	if( HitTest(pt) )
	{
		if(m_pCheckAll->HitTest(pt))
		{		
			for(int i= 0;i<m_pList->GetItemCnt();i++)
			{
				CTMailItem * pItem = static_cast<CTMailItem*>(m_pList->GetItem(i));
				if(pItem->pSimple->m_bRead && pItem->pSimple->m_bContain == 0)
					pItem->SelectToDel = TRUE;
			}
		}
	}

	CTClientUIBase::OnLButtonUp(nFlags, pt);
}
// -------------------------------------------------------
void CTMailListFrame::OnRButtonDown(UINT nFlags, CPoint pt)
{
}
// -------------------------------------------------------
void CTMailListFrame::SetInfo(WORD wTotalCount, WORD wNotReadCount, WORD wCurPage)
{
	m_wTotalCount = wTotalCount;
//	m_wCurPage = wCurPage;
	m_pMailDlgTitle->m_strText = CTChart::Format( TSTR_MAIL_TITLE, wNotReadCount, wTotalCount);

	/*if( m_wTotalCount != 0 && m_wCurPage != 0 )
	{
		INT nTotalPageCount = wTotalCount / 7 + (wTotalCount%7 != 0 ? 1 : 0);
		INT nTotalPageGroupCount = nTotalPageCount / 5;
		INT nCurPageGroup = (wCurPage-1) / 5;

		if( nCurPageGroup-1 >= 0 )
		{
			m_pPage[0]->m_strText = CTChart::LoadString( TSTR_MAIL_PREVPAGE );
			m_pPage[0]->SetTextClr( D3DCOLOR_XRGB(210, 210, 210) );
		}
		else
			m_pPage[0]->m_strText.Empty();

		if( nCurPageGroup+1 < nTotalPageGroupCount )
		{
			m_pPage[6]->m_strText = CTChart::LoadString( TSTR_MAIL_NEXTPAGE );
			m_pPage[6]->SetTextClr( D3DCOLOR_XRGB(210, 210, 210) );
		}
		else
			m_pPage[6]->m_strText.Empty();

		INT nPage = nCurPageGroup * 5 + 1; // first page in current page group.
		for( int i=1 ; i <= 5 ; ++i )
		{
			if( nPage <= nTotalPageCount )
			{
				m_pPage[i]->m_strText.Format("%d", nPage);

				if( nPage == wCurPage )
					m_pPage[i]->SetTextClr( D3DCOLOR_XRGB(254, 208, 0) );
				else
					m_pPage[i]->SetTextClr( D3DCOLOR_XRGB(210, 210, 210) );

				++nPage;
			}
			else
				m_pPage[i]->m_strText.Empty();
		}
	}
	else
	{
		for( int i=0 ; i < 7 ; ++i )
		{
			m_pPage[i]->m_strText.Empty();
		}
	}*/
}
// =======================================================

// =======================================================

// =======================================================
void CTMailSlot::ShowComponent(BOOL bShow)
{
	
	m_bShow = bShow;	
	m_pBase->ShowComponent( FALSE );
	if ( bShow )
	{
		m_pButton->ShowComponent( TRUE );

		m_pSenderTxt->ShowComponent( TRUE );
		m_pTitleTxt->ShowComponent( TRUE );
		m_pTimeTxt->ShowComponent( TRUE );

	}
	else
	{
		m_pButton->ShowComponent( FALSE );
		m_pMailImg->ShowComponent( FALSE );
		m_pPackage->ShowComponent( FALSE );

		m_pSenderTxt->ShowComponent( FALSE );
		m_pTitleTxt->ShowComponent( FALSE );
		m_pTimeTxt->ShowComponent( FALSE );
		m_pCheck->ShowComponent( FALSE );

	}
}
// ----------------------------------------------------------------------------------------------------
void CTMailSlot::Select(BOOL bSel)
{
	static_cast<TButton*>(m_pButton)->Select(bSel);
}
void CTMailItem::ReflectSlot(CTCtrlListSlot* pSlots)
{
	CTMailSlot* pSlot = static_cast<CTMailSlot*>(pSlots);

	static const DWORD MAIL_TYPE_STR[] = 
	{
		TSTR_MAILTYPE_NORMAL,	//POST_NORMAL
		TSTR_MAILTYPE_PACKATE,	//POST_PACKATE
		TSTR_MAILTYPE_BILLS,		//POST_BILLS
		TSTR_MAILTYPE_RETURN,	//POST_RETURN
		TSTR_MAILTYPE_PAYMENT,	//POST_PAYMENT
		TSTR_MAILTYPE_NPC,		//POST_NPC
		TSTR_MAILTYPE_OPERATOR,	//POST_OPERATOR
		TSTR_MAILTYPE_CASH		//POST_CASH
	};
	pSlot->m_pMail = pSimple;

	if( pSlot->m_pMail )
	{
		CTTextLinker *pTextLinker = CTTextLinker::GetInstance();

		//pSlot->m_pTypeTxt->m_strText = CTChart::LoadString( (TSTRING) MAIL_TYPE_STR[pSimple->m_bType] );


		if(pSimple->m_bRead)
		{			
			pSlot->m_pCheck->ShowComponent( FALSE );		
			pSlot->m_pMailImg->ShowComponent( FALSE );
			pSlot->m_pPackage->ShowComponent( FALSE );

			if(pSimple->m_bContain)		
				pSlot->m_pPackage->ShowComponent( TRUE );
			else
				pSlot->m_pCheck->ShowComponent( TRUE );	
		}
		else
		{					
			pSlot->m_pMailImg->ShowComponent( FALSE );
			pSlot->m_pPackage->ShowComponent( FALSE );
			pSlot->m_pCheck->ShowComponent( FALSE );	
			if(pSimple->m_bType==0)				
				pSlot->m_pMailImg->ShowComponent( TRUE );
			else
				pSlot->m_pPackage->ShowComponent( TRUE );
		}



		pSlot->m_pSenderTxt->m_strText = pSimple->m_strSender;

		pSlot->m_pTitleTxt->ResetTextSetting();
		pSlot->m_pTitleTxt->m_strText = pTextLinker->MakeNetToSimpleLinkText( pSlot->m_pTitleTxt, pSimple->m_strTitle);

		if(pSlot->m_pTitleTxt->m_strText.IsEmpty())
			pSlot->m_pTitleTxt->m_strText = CTChart::LoadString( TSTR_FMT_NOTITLE);

		CTime time(pSimple->m_nTime);
		pSlot->m_pTimeTxt->m_strText = CTChart::Format( TSTR_FMT_DATE, time.GetMonth(),time.GetDay());

		DWORD dwColor = pSlot->m_pMail->m_bRead? READ_MAIL_COLOR: NEW_MAIL_COLOR;
//		pSlot->m_pTypeTxt->SetTextClr(dwColor);
		pSlot->m_pSenderTxt->SetTextClr(dwColor);
		pSlot->m_pTitleTxt->SetTextClr(dwColor);
		pSlot->m_pTimeTxt->SetTextClr(dwColor);
	}
}
// ----------------------------------------------------------------------------------------------------
