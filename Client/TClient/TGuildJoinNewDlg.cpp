#include "StdAfx.h"
#include "TFameRank.h"
#include "TGuildJoinNewDlg.h"
#include "TClientGame.h"
#include "Resource.h"
#include "TClient.h"
#include "TMailSendDlg.h"
BYTE CTGuildJoinNewDlg::m_bTabIndex = TGUILD_JOIN;

CTGuildJoinNewDlg::PopupBtnInfo CTGuildJoinNewDlg::m_PopupBtnInfo[7] = 
{
//	{m_dwTXT,						m_dwGM	},
	{TSTR_GVFPM_MAIL,						GM_GVFPM_MAIL					},	///< GVFPM_MAIL
	{TSTR_GVFPM_MAIL,						GM_GVFPM_MAIL2					},	///< GVFPM_MAIL2
	{TSTR_GVFPM_REGULAR_ACCEPT,		GM_GVFPM_REGULAR_ACCEPT	},	///< GVFPM_REGULAR_ACCEPT
	{TSTR_GVFPM_REGULAR_REJECT,		GM_GVFPM_REGULAR_REJECT	},	///< GVFPM_REGULAR_REJECT
	{TSTR_GVFPM_TACTICS_ACCEPT,			GM_GVFPM_TACTICS_ACCEPT	},	///< GVFPM_TACTICS_ACCEPT
	{TSTR_GVFPM_TACTICS_REJECT,			GM_GVFPM_TACTICS_REJECT		},	///< GVFPM_TACTICS_REJECT
	{TSTR_GVFPM_CLOSE,						GM_GVFPM_CLOSE					},	///< GVFPM_CLOSE
};


CTGuildJoinNewDlg::CTGuildJoinNewDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc, FRAMEDESC_SHAREDPTR pPopupDesc)
	: ITInnerFrame(pParent, pDesc, TGUILD_JOIN),
		m_nLastSelType(T_INVALID),
		m_nLastSelIdx(T_INVALID),
		m_nLastSelType2(T_INVALID),
		m_nLastSelIdx2(T_INVALID)
{
	m_pList = (TList*) FindKid( ID_CTRLINST_LIST );
	m_pList2 = (TList*) FindKid( 27700 );
	m_pPopup = new CTMiniPopupDlg(this ,CTClientGame::GetInstance()->m_pTParser->FindFrameTemplate(ID_FRAME_ACTLIST_NEW), FALSE);
	m_pPopup->m_id = GetUniqueID();
	m_bPopupVisible = FALSE;
	m_pDownItem = NULL;
}

CTGuildJoinNewDlg::~CTGuildJoinNewDlg()
{
	if( m_pPopup )
		delete m_pPopup;
}

void CTGuildJoinNewDlg::RequestInfo()
{
	CTClientGame* pGame = CTClientGame::GetInstance();
	CTClientSession* pSESSION = pGame->GetSession();
	CTClientChar* pMainChar = pGame->GetMainChar();

	if( pMainChar->m_dwGuildID == 0 && pMainChar->m_dwTacticsID == 0 )
		return ;

		if( pSESSION )
		{
			pSESSION->SendCS_GUILDVOLUNTEERLIST_REQ();
			pSESSION->SendCS_GUILDTACTICSVOLUNTEERLIST_REQ();
		}
}

void CTGuildJoinNewDlg::ResetInfo()
{
	CTGuildCommander* pGuildCmd = CTGuildCommander::GetInstance();

			m_pList->RemoveAll();

			CTGuildCommander::GuildVolunteerVec::iterator itMEMBER, end;
			itMEMBER = pGuildCmd->m_GuildVolunteerVec.begin();
			end = pGuildCmd->m_GuildVolunteerVec.end();

			for(; itMEMBER != end ; ++itMEMBER )
			{
				int nLine = m_pList->AddString("");

				for(int i=0; i<GRVI_COUNT; ++i)
				{
					switch( i )
					{
					case GRVI_NAME:
						{
							m_pList->SetItemString( nLine, 1, itMEMBER->m_strName );
							m_pList->SetItemData( nLine, 1, MAKELONG( nLine, 1) );
							m_pList->SetColumnAlign( 1, ALIGN_CENTER );
						}
						break;

					case GRVI_CLASS:
						{
							m_pList->SetItemData( nLine, 2, MAKELONG( nLine, 2) );
							m_pList->SetImageIndex( nLine, 2, TRUE, itMEMBER->m_bClass);
							m_pList->SetColumnAlign( 2, ALIGN_CENTER );
						}
						break;

					case GRVI_LEVEL	:
						{
							CString strLevel;
							strLevel.Format("Lv %d", itMEMBER->m_bLevel);
							m_pList->SetItemString( nLine, 3, strLevel);

							m_pList->SetItemData( nLine, 3, MAKELONG( nLine, 3) );
							m_pList->SetColumnAlign( 3, ALIGN_CENTER );
						}
						break;
					}
				}
			}

			m_pList2->RemoveAll();

			CTGuildCommander::GuildTacticsVolunteerVec::iterator itMEMBERR, endd;
			itMEMBERR = pGuildCmd->m_TacticsVolunteerVec.begin();
			endd = pGuildCmd->m_TacticsVolunteerVec.end();

			for(; itMEMBERR != endd ; ++itMEMBERR )
			{
				int nLine = m_pList2->AddString("");

				for(int i=0; i<GTVI_COUNT; ++i)
				{
					switch( i )
					{
					case GTVI_NAME:
						{
							m_pList2->SetItemString( nLine, 1, itMEMBERR->m_strName );
							m_pList2->SetItemData( nLine, 1, MAKELONG( nLine, 1) );
							m_pList2->SetColumnAlign( 1, ALIGN_CENTER );
						}
						break;

					case GTVI_CLASS:
						{
							m_pList2->SetItemData( nLine, 2, MAKELONG( nLine, 2) );
							m_pList2->SetImageIndex( nLine, 2, TRUE, itMEMBERR->m_bClass);
							m_pList2->SetColumnAlign( 2, ALIGN_CENTER );
						}
						break;

					case GTVI_LEVEL	:
						{
							CString strLevel;
							strLevel.Format("Lv %d", itMEMBERR->m_bLevel);

							m_pList2->SetItemString( nLine, 3, strLevel );
							m_pList2->SetItemData( nLine, 3, MAKELONG( nLine, 3) );
							m_pList2->SetColumnAlign( 3, ALIGN_CENTER );
						}
						break;
					}
				}
			}
}


void CTGuildJoinNewDlg::OnLButtonDown( UINT nFlags, CPoint pt )
{
	if( !m_bPopupVisible || !m_pPopup->HitTest(pt) )
		 if(m_pList->GetHitItem(pt))
			m_pDownItem = m_pList->GetHitItem(pt);
		else if(m_pList2->GetHitItem( pt))
			m_pDownItem = m_pList2->GetHitItem( pt);

	ITInnerFrame::OnLButtonDown( nFlags, pt );
}

void CTGuildJoinNewDlg::OnLButtonUp( UINT nFlags, CPoint pt )
{
	ITInnerFrame::OnLButtonUp( nFlags, pt );

	TListItem* pDownItem = m_pDownItem;
	m_pDownItem = NULL;

	if( !m_bPopupVisible || !m_pPopup->HitTest(pt) )
	{
		if(m_pList->GetHitItem( pt))
		{
			TListItem* pItem = m_pList->GetHitItem( pt);
			if( pItem != pDownItem )
				return;

			if( pItem )
			{
				m_nLastSelIdx 	= LOWORD(pItem->m_param);
				m_nLastSelType	= HIWORD(pItem->m_param);
			}
			else
			{
				m_nLastSelIdx	= T_INVALID;
				m_nLastSelType	= T_INVALID;
			}

			ShowPopup(pt);
		}		
		else if(m_pList2->GetHitItem( pt))
		{
			TListItem* pItem = m_pList2->GetHitItem( pt);
			if( pItem != pDownItem )
				return;

			if( pItem )
			{
				m_nLastSelIdx2 	= LOWORD(pItem->m_param);
				m_nLastSelType2	= HIWORD(pItem->m_param);
			}
			else
			{
				m_nLastSelIdx2	= T_INVALID;
				m_nLastSelType2	= T_INVALID;
			}

			ShowPopup2(pt);
		}
		else
		{
			CancelPopup();
		}
	}
}

void CTGuildJoinNewDlg::ShowComponent(BOOL bVisible)
{
	ITInnerFrame::ShowComponent( bVisible );
}

void CTGuildJoinNewDlg::ShowPopup(const CPoint& pt)
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return;


	/*if( m_nLastSelIdx == T_INVALID )
		return;*/
	for( INT i=0; i < 4 ; ++i )
	{
		BOOL bADD = FALSE;

		switch( i )
		{
			case GVFPM_MAIL:
				{
					if(m_pList->HitTest(pt))
					bADD = FALSE;
				}
				break;
			case GVFPM_REGULAR_ACCEPT:
				{
					if(m_pList->HitTest(pt))
						bADD = TRUE;
				}
				break;
			case GVFPM_REGULAR_REJECT:
				{
					if(m_pList->HitTest(pt))
						bADD = TRUE;
				}
				break;
		}

		if( bADD )
		{
			CString strTITLE;
			strTITLE = CTChart::LoadString( (TSTRING) m_PopupBtnInfo[i].m_dwTXT);
			m_pPopup->AddButton(strTITLE, m_PopupBtnInfo[i].m_dwGM);
		}
	}

	if( !m_pPopup->IsButtonEmpty() )
	{
		CString strTITLE;
		strTITLE = CTChart::LoadString( (TSTRING) m_PopupBtnInfo[GVFPM_CLOSE].m_dwTXT);
		m_pPopup->AddButton(strTITLE, m_PopupBtnInfo[GVFPM_CLOSE].m_dwGM);
	
		AddKid(m_pPopup);

		CRect rc = m_rc;
		ComponentToScreen(&rc);
		CPoint ptPOP = pt - rc.TopLeft();

		m_pPopup->MoveComponent(ptPOP);
		m_pPopup->ShowComponent(TRUE);

		m_bPopupVisible = TRUE;
	}
}

void CTGuildJoinNewDlg::ShowPopup2(const CPoint& pt)
{
	CancelPopup();

	if( m_nLastSelIdx2 == T_INVALID )
		return;


	/*if( m_nLastSelIdx == T_INVALID )
		return;*/
	for( INT i=0; i < 7 ; ++i )
	{
		BOOL bADD = FALSE;

		switch( i )
		{
			case GVFPM_MAIL2:
				{
					if(m_pList2->HitTest(pt))
					bADD = FALSE;
				}
				break;
			case GVFPM_TACTICS_ACCEPT:
				{
					if(m_pList2->HitTest(pt))
						bADD = TRUE;
				}
				break;
			case GVFPM_TACTICS_REJECT:
				{
					if(m_pList2->HitTest(pt))
						bADD = TRUE;
				}
				break;
		}

		if( bADD )
		{
			CString strTITLE;
			strTITLE = CTChart::LoadString( (TSTRING) m_PopupBtnInfo[i].m_dwTXT);
			m_pPopup->AddButton(strTITLE, m_PopupBtnInfo[i].m_dwGM);
		}
	}

	if( !m_pPopup->IsButtonEmpty() )
	{
		CString strTITLE;
		strTITLE = CTChart::LoadString( (TSTRING) m_PopupBtnInfo[GVFPM_CLOSE].m_dwTXT);
		m_pPopup->AddButton(strTITLE, m_PopupBtnInfo[GVFPM_CLOSE].m_dwGM);
	
		AddKid(m_pPopup);

		CRect rc = m_rc;
		ComponentToScreen(&rc);
		CPoint ptPOP = pt - rc.TopLeft();

		m_pPopup->MoveComponent(ptPOP);
		m_pPopup->ShowComponent(TRUE);

		m_bPopupVisible = TRUE;
	}
}

void CTGuildJoinNewDlg::CancelPopup()
{
	RemoveKid(m_pPopup);
	m_pPopup->ClearButtons();
	m_bPopupVisible = FALSE;
}

int CTGuildJoinNewDlg::OnGM_GVFPM_MAIL()	///< GVFPM_MAIL
{
	CancelPopup();

	/*if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTClientGame* pGame = CTClientGame::GetInstance();
	if( !pGame->CanUseMail() )
		return TERR_MAIL_REGION;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();

	CString strCHAR;
	strCHAR = pCmd->m_GuildVolunteerVec[ m_nLastSelIdx ].m_strName;

	CTMailSendDlg* pSndDlg = static_cast<CTMailSendDlg*>(pGame->GetFrame(TFRAME_MAIL_SEND));
	pSndDlg->SetReceiver(strCHAR);
	pSndDlg->SetTitle("");

	pGame->EnableUI(TFRAME_MAIL_SEND);*/
	return TERR_NONE;
}

int CTGuildJoinNewDlg::OnGM_GVFPM_REGULAR_ACCEPT()	///< GVFPM_REGULAR_ACCEPT
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	CTClientGame* pGame = CTClientGame::GetInstance();

	if( pGame->GetSession() )
	{
			pGame->GetSession()->SendCS_GUILDVOLUNTEERREPLY_REQ(
				pCmd->m_GuildVolunteerVec[ m_nLastSelIdx ].m_dwCharID,
				TRUE );
	}

	return TERR_NONE;
}

int CTGuildJoinNewDlg::OnGM_GVFPM_REGULAR_REJECT()	///< GVFPM_REGULAR_REJECT
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	CTClientGame* pGame = CTClientGame::GetInstance();

	if( pGame->GetSession() )
	{
			pGame->GetSession()->SendCS_GUILDVOLUNTEERREPLY_REQ(
				pCmd->m_GuildVolunteerVec[ m_nLastSelIdx ].m_dwCharID,
				FALSE );
	}

	return TERR_NONE;
}

int CTGuildJoinNewDlg::OnGM_GVFPM_MAIL2()	///< GVFPM_MAIL
{
	CancelPopup();

	//if( m_nLastSelIdx == T_INVALID )
	//	return TERR_NONE;

	/*if( m_nLastSelIdx2 == T_INVALID )
		return TERR_NONE;

	CTClientGame* pGame = CTClientGame::GetInstance();
	if( !pGame->CanUseMail() )
		return TERR_MAIL_REGION;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();

	CString strCHAR;
	strCHAR = pCmd->m_TacticsVolunteerVec[ m_nLastSelIdx2 ].m_strName;

	CTMailSendDlg* pSndDlg = static_cast<CTMailSendDlg*>(pGame->GetFrame(TFRAME_MAIL_SEND));
	pSndDlg->SetReceiver(strCHAR);
	pSndDlg->SetTitle("");

	pGame->EnableUI(TFRAME_MAIL_SEND);*/
	return TERR_NONE;
}


int CTGuildJoinNewDlg::OnGM_GVFPM_TACTICS_ACCEPT()	///< GVFPM_TACTICS_ACCEPT
{
	CancelPopup();

	if( m_nLastSelIdx2 == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	CTClientGame* pGame = CTClientGame::GetInstance();

	if( pGame->GetSession() )
	{
		pGame->GetSession()->SendCS_GUILDTACTICSREPLY_REQ(
			pCmd->m_TacticsVolunteerVec[ m_nLastSelIdx2 ].m_dwCharID,
			TRUE );
	}

	return TERR_NONE;
}

int CTGuildJoinNewDlg::OnGM_GVFPM_TACTICS_REJECT()	///< GVFPM_TACTICS_REJECT
{
	CancelPopup();

	if( m_nLastSelIdx2 == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	CTClientGame* pGame = CTClientGame::GetInstance();

	if( pGame->GetSession() )
	{
		pGame->GetSession()->SendCS_GUILDTACTICSREPLY_REQ(
			pCmd->m_TacticsVolunteerVec[ m_nLastSelIdx2 ].m_dwCharID,
			FALSE );
	}

	return TERR_NONE;
}

int CTGuildJoinNewDlg::OnGM_GVFPM_CLOSE()	///< GVFPM_CLOSE
{
	CancelPopup();
	return TERR_NONE;
}

HRESULT CTGuildJoinNewDlg::Render( DWORD dwTickCount)
{
	return ITInnerFrame::Render( dwTickCount );
}
