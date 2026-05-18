#include "StdAfx.h"
#include "TFameRank.h"
#include "TGuildMercenaryNewDlg.h"
#include "TClientGame.h"
#include "Resource.h"
#include "TClient.h"
#include "TGuildPointRewardFrame.h"
#include "TMailSendDlg.h"
#include "TTacticsInviteDlg.h"
BYTE CTGuildMercenaryNewDlg::m_bTabIndex = TGUILD_MERCENARY;

const CTGuildMercenaryNewDlg::PopupBtnInfo CTGuildMercenaryNewDlg::m_PopupBtnInfo[TGM_PBTN_COUNT] = 
{
	{TSTR_GMP_WHISPER,				GM_GMP_WHISPER2			},	///< TGM_PBTN_WHISPER,		
	{TSTR_GMP_MAIL,					GM_GMP_MAIL				},	///< TGM_PBTN_MAIL,			
	{TSTR_GMP_POINT_REWARD,			GM_GMP_POINT_REWARD		},	///< TGM_PBTN_POINT_REWARD,
	{TSTR_GMP_PARTY,				GM_GMP_PARTY			},	///< TGM_PBTN_PARTY
	{TSTR_GMP_FRIEND,				GM_GMP_FRIEND			},	///< TGM_PBTN_FRIEND
	{TSTR_GMP_KICK,					GM_GMP_KICK				},	///< TGM_PBTN_KICK,	
	{TSTR_GMP_ET_GM,				GM_GMP_ET_GM			},	///< TGM_PBTN_SET_GM,		
	{TSTR_GMP_SET_GSM,				GM_GMP_SET_GSM			},	///< TGM_PBTN_SET_GSM,		
	{TSTR_GMP_CANCEL_GSM,			GM_GMP_CANCEL_GSM		},	///< TGM_PBTN_CANCEL_GSM,	
	{TSTR_GMP_PEER_BARON,			GM_GMP_PEER_BARON		},	///< TGM_PBTN_PEER_BARON,	
	{TSTR_GMP_PEER_VISCOUNT,		GM_GMP_PEER_VISCOUNT	},	///< TGM_PBTN_PEER_VISCOUNT,
	{TSTR_GMP_PEER_COUNT,			GM_GMP_PEER_COUNT		},	///< TGM_PBTN_PEER_COUNT,	
	{TSTR_GMP_PEER_MARQUIS,			GM_GMP_PEER_MARQUIS		},	///< TGM_PBTN_PEER_MARQUIS,	
	{TSTR_GMP_PEER_DUKE,			GM_GMP_PEER_DUKE		},	///< TGM_PBTN_PEER_DUKE,	
	{TSTR_GMP_CANCEL_PEER,			GM_GMP_CANCEL_PEER		},	///< TGM_PBTN_CANCEL_PEER,	
	{TSTR_GMP_CANCEL,				GM_GMP_CANCEL			},	///< TGM_PBTN_CANCEL,	
	{TSTR_GMP_VOLUNTEER_HESED,		GM_GMP_VOLUNTEER_HESED	},	///< PBTN_VOLUNTEER_HESED,
	{TSTR_GMP_VOLUNTEER_ARDRI,		GM_GMP_VOLUNTEER_ARDRI	},
	{TSTR_GMP_VOLUNTEER_TYCON,		GM_GMP_VOLUNTEER_TYCON	},
	{TSTR_GMP_VOLUNTEER_GEHBRA,		GM_GMP_VOLUNTEER_GEHBRA	},
	{TSTR_GMP_VOLUNTEER_CANCEL,		GM_GMP_VOLUNTEER_CANCEL	},

	{TSTR_GMP_TACTICS_RE_CONTRACT,	GM_GMP_TACTICS_RE_CONTRACT	}, ///< TGM_PBTN_TACTICS_RE_CONTRACT,			///< 용병 재계약
	{TSTR_GMP_TACTICS_KICK,			GM_GMP_TACTICS_KICK			}, ///< TGM_PBTN_TACTICS_KICK,						///< 용병 계약 파기
	{TSTR_GMP_TACTICS_REWARD_POINT,	GM_GMP_TACTICS_REWARD_POINT	}, ///< TGM_PBTN_TACTICS_REWARD_POINT,			///< 용병 포상
	{TSTR_GMP_TACTICS_SELF_KICK,		GM_GMP_TACTICS_SELF_KICK } //TGM_PBTN_TACTICS_SELF_KICK,					///< 용병 탈퇴
};


CTGuildMercenaryNewDlg::CTGuildMercenaryNewDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc, FRAMEDESC_SHAREDPTR pPopupDesc)
	: ITInnerFrame(pParent, pDesc, TGUILD_MERCENARY),
		m_nLastSelType(T_INVALID),
		m_nLastSelIdx(T_INVALID),
		m_bPopupVisible(FALSE),
		m_pDownItem(NULL),
		m_pPopup(NULL)
{
	m_pListC = (TList*) FindKid( ID_CTRLINST_LIST_C );
	m_pListC->ApplyUserColor( TRUE );

	m_pPopup = new CTMiniPopupDlg(this ,CTClientGame::GetInstance()->m_pTParser->FindFrameTemplate(ID_FRAME_ACTLIST_NEW), FALSE);
	m_pPopup->m_id = GetUniqueID();	
}

CTGuildMercenaryNewDlg::~CTGuildMercenaryNewDlg()
{
	if( m_pPopup )
		delete m_pPopup;
}

void CTGuildMercenaryNewDlg::RequestInfo()
{
	CTClientGame* pGame = CTClientGame::GetInstance();
	if( !pGame )
		return;

	CTClientSession* pSession = pGame->GetSession();
	if( !pSession )
		return;

	CTClientChar* pMainChar = pGame->GetMainChar();
	if( !pMainChar )
		return;

	if( pMainChar->m_dwGuildID == 0 && pMainChar->m_dwTacticsID == 0 )
		return ;

		if(pSession)
		{
			pSession->SendCS_GUILDLOCALLIST_REQ();
			pSession->SendCS_GUILDTACTICSLIST_REQ();
		}
}

void CTGuildMercenaryNewDlg::ResetInfo()
{
	CTGuildCommander* pGuildCmd = CTGuildCommander::GetInstance();
		pGuildCmd->SortTactics();

		int nTop = m_pListC->GetTop();

		m_pListC->RemoveAll();

		CTGuildCommander::GuildTacticsVec::iterator itMEMBER, end;
		itMEMBER = pGuildCmd->m_GuildTacticsVec.begin();
		end = pGuildCmd->m_GuildTacticsVec.end();

		for(; itMEMBER != end ; ++itMEMBER )
		{
			int nLine = m_pListC->AddString("");

			DWORD dwColor = COLOR_CASTLE_NONE;
			switch( itMEMBER->m_wCastle )
			{
			case TCASTLE_HESED : dwColor = COLOR_CASTLE_HESED; break;
			case TCASTLE_ARDRI : dwColor = COLOR_CASTLE_ARDRI; break;
			case TCASTLE_GEHBRA : dwColor = COLOR_CASTLE_GEHBRA; break;
			case TCASTLE_TYCON : dwColor = COLOR_CASTLE_TYCON; break;
			case TCASTLE_WOODLAND: dwColor = COLOR_CASTLE_WOODLAND; break;
			default : dwColor = COLOR_CASTLE_NONE; break;
			}

			for(int i=0; i<TACTICS_INFO_COUNT; ++i)
			{
				CString strText;

				switch( i )
				{
				case TACTICS_INFO_NAME:
					{
						m_pListC->SetItemString( nLine, 1, itMEMBER->m_strName );
						m_pListC->SetItemData( nLine, 1, MAKELONG( nLine, TACTICS_INFO_NAME) );
						m_pListC->SetUserColor( nLine, 1, dwColor );
						m_pListC->SetColumnAlign( 1, ALIGN_CENTER );
					}
					break;

				case TACTICS_INFO_CLASS:
					{
						/*m_pListC->SetItemString( nLine, 2,
							CTChart::LoadString( (TSTRING) CTClientGame::m_vTCLASSSTR[ itMEMBER->m_bClass] ) );*/

						m_pListC->SetItemData( nLine, 2, MAKELONG( nLine, 2) );
						m_pListC->SetImageIndex( nLine, 2, TRUE, itMEMBER->m_bClass);
						m_pListC->SetUserColor( nLine, 2, dwColor );
						m_pListC->SetColumnAlign( 2, ALIGN_CENTER );
					}
					break;

				case TACTICS_INFO_LEVEL	:
					{
						m_pListC->SetItemString( nLine, 3,
								CTChart::Format( TSTR_FMT_NUMBER, itMEMBER->m_bLevel) );
						m_pListC->SetItemData( nLine, 3, MAKELONG( nLine, 3) );
						m_pListC->SetUserColor( nLine, 3, dwColor );
						m_pListC->SetColumnAlign( 3, ALIGN_CENTER );
					}
					break;

				case TACTICS_INFO_DAY:
					{
						/*CTimeSpan timeSpan = CTime(itMEMBER->m_dlEndTime) - CTClientApp::m_dCurDate;
						DWORD dwDays = (DWORD) timeSpan.GetDays();

						CString strFMT;
						strFMT.Format( "%d/%d", dwDays, itMEMBER->m_bDay );*/

						CString strFMT;
						CTime t(itMEMBER->m_dlEndTime);
						strFMT = CTChart::Format( TSTR_GUILD_ENDTIME,
							t.GetYear(),
							t.GetMonth(),
							t.GetDay(),
							t.GetHour(),
							t.GetMinute() );

						m_pListC->SetItemString( nLine, 4, strFMT );
						m_pListC->SetItemData( nLine, 4, MAKELONG( nLine, 4) );
						m_pListC->SetUserColor( nLine, 4, dwColor );
						m_pListC->SetColumnAlign( 4, ALIGN_CENTER );
					}
					break;

				case TACTICS_INFO_POINT:
					{
						m_pListC->SetItemString( nLine, 5,
							CTChart::Format( TSTR_FMT_NUMBER, itMEMBER->m_dwPoint ) );

						m_pListC->SetItemData( nLine, 5, MAKELONG( nLine, 5) );
						m_pListC->SetUserColor( nLine, 5, dwColor );
						m_pListC->SetColumnAlign( 5, ALIGN_CENTER );
					}
					break;


				case TACTICS_INFO_GOTPOINT:
					{		
					/*	m_pListC->SetItemString( nLine, 6,
							CTChart::Format( TSTR_FMT_NUMBER, itMEMBER->m_dwGotPoint ) );

						m_pListC->SetItemData( nLine, 6, MAKELONG( nLine, 6) );
						m_pListC->SetUserColor( nLine, 6, dwColor );
						m_pListC->SetColumnAlign( 6, ALIGN_CENTER );*/

						if( itMEMBER->m_dwRegion != 0 )
						{
							LPTREGIONINFO pRegion = CTChart::FindTREGIONINFO( itMEMBER->m_dwRegion);

							if( pRegion )
								m_pListC->SetItemString( nLine, 6, pRegion->m_strNAME);
						}
						else
						{
							m_pListC->SetItemString( nLine, 6, CTChart::LoadString( TSTR_STATE_DISCONNECT));
						}

						m_pListC->SetItemData( nLine, 6, MAKELONG( nLine, 6) );
						m_pListC->SetUserColor( nLine, 6, dwColor );
						m_pListC->SetColumnAlign( 6, ALIGN_CENTER );
					}
					break;

			/*	case TACTICS_INFO_REGION:
					{
						if( itMEMBER->m_dwRegion != 0 )
						{
							LPTREGIONINFO pRegion = CTChart::FindTREGIONINFO( itMEMBER->m_dwRegion);

							if( pRegion )
								m_pListC->SetItemString( nLine, 7, pRegion->m_strNAME);
						}
						else
						{
							m_pListC->SetItemString( nLine, 7, CTChart::LoadString( TSTR_STATE_DISCONNECT));
						}

						m_pListC->SetItemData( nLine, 7, MAKELONG( nLine, 7) );
						m_pListC->SetUserColor( nLine, 7, dwColor );
						m_pListC->SetColumnAlign( 7, ALIGN_CENTER );
					}
					break;*/
				}
			}
		}

		RemoveKid( m_pListC );
		AddKid( m_pListC );
		//m_pListC->SetCurSelItem( nTop );
}


void CTGuildMercenaryNewDlg::OnLButtonDown( UINT nFlags, CPoint pt )
{
	if( !IsPopupVisible() || !m_pPopup->HitTest(pt) )
	{
		m_pDownItem = m_pListC->GetHitItem( pt);
	}

	ITInnerFrame::OnLButtonDown( nFlags, pt );
}

void CTGuildMercenaryNewDlg::OnLButtonUp( UINT nFlags, CPoint pt )
{
	ITInnerFrame::OnLButtonUp( nFlags, pt );

	TListItem* pDownItem = m_pDownItem;
	m_pDownItem = NULL;

	if( !IsPopupVisible() || !m_pPopup->HitTest(pt) )
	{
		TListItem* pItem = NULL;

		pItem = m_pListC->GetHitItem( pt);	

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
}

void CTGuildMercenaryNewDlg::ShowPopup(const CPoint& pt)
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return;

	CTClientGame* pGame = CTClientGame::GetInstance();
	CTClientChar* pMainChar = CTClientGame::GetInstance()->GetMainChar();
	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	const GuildDetInfo& detif = pCmd->GetGuildDetInfo();

	for(INT i=0; i<TGM_PBTN_COUNT; ++i)
	{
		BOOL bADD = FALSE;

		switch(i)
		{
		case TGM_PBTN_WHISPER		:
		case TGM_PBTN_MAIL			:
			{
				 if( m_nLastSelType == TACTICS_INFO_NAME  )
					bADD = TRUE;
			}
			break;

		case TGM_PBTN_PARTY			:
			if( m_nLastSelType == RINFO_NAME )
			{
				GuildTactics* pInfo = pCmd->GetTacticsPtr( m_nLastSelIdx );
				if( pInfo->m_dwRegion != 0 )
				{
					WORD wSquadID = pGame->GetPartyID(pMainChar);
					if( (!wSquadID || (
						pMainChar->m_dwChiefID == pMainChar->m_dwID )) )
						bADD = TRUE;
				}
			}

			break;

		case TGM_PBTN_FRIEND			:
			if( m_nLastSelType == RINFO_NAME )
			{
				GuildTactics* pInfo = pCmd->GetTacticsPtr( m_nLastSelIdx );
				if( pInfo->m_dwRegion != 0 )
				{
					bADD = TRUE;
				}
			}
			break;

		case TGM_PBTN_VOLUNTEER_CANCEL	:
			if( pMainChar->m_bGuildDuty == GUILD_DUTY_CHIEF && pMainChar->m_dwTacticsID == 0 && m_nLastSelType == RINFO_NAME )
			{
				GuildTactics* info = pCmd->GetTacticsPtr( m_nLastSelIdx );
				if( info->m_wCastle )
					bADD = TRUE;
			}
			break;
		case TGM_PBTN_VOLUNTEER_HESED : bADD = ShowPopupCastle( TCASTLE_HESED ); break;
		case TGM_PBTN_VOLUNTEER_ARDRI : bADD = ShowPopupCastle( TCASTLE_ARDRI ); break;
		case TGM_PBTN_VOLUNTEER_GEHBRA : bADD = ShowPopupCastle( TCASTLE_GEHBRA ); break;
		case TGM_PBTN_VOLUNTEER_TYCON : bADD = ShowPopupCastle( TCASTLE_TYCON ); break;

		case TGM_PBTN_TACTICS_RE_CONTRACT: ///< 용병 재계약
			if( m_nLastSelType == RINFO_NAME )
			{
				if( pMainChar->m_bGuildDuty == GUILD_DUTY_CHIEF && pMainChar->m_dwTacticsID == 0 )
				{
					bADD = TRUE;
				}
			}
			break;

		case TGM_PBTN_TACTICS_KICK: ///< 용병 계약 파기
			if( m_nLastSelType == RINFO_NAME )
			{
				if( pMainChar->m_bGuildDuty == GUILD_DUTY_CHIEF && pMainChar->m_dwTacticsID == 0 )
				{
					bADD = TRUE;
				}
			}
			break;

		case TGM_PBTN_TACTICS_SELF_KICK:
			{
				if( m_nLastSelType == RINFO_NAME )
				{
					GuildTactics* info = pCmd->GetTacticsPtr( m_nLastSelIdx );
					if( info &&
						pMainChar->m_dwTacticsID != 0 &&
						pMainChar->m_dwID == info->m_dwCharID )
					{
						bADD = TRUE;
					}
				}
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
		strTITLE = CTChart::LoadString( (TSTRING) m_PopupBtnInfo[TGM_PBTN_CANCEL].m_dwTXT);
		m_pPopup->AddButton(strTITLE, m_PopupBtnInfo[TGM_PBTN_CANCEL].m_dwGM);
	
		AddKid(m_pPopup);

		CRect rc = m_rc;
		ComponentToScreen(&rc);
		CPoint ptPOP = pt - rc.TopLeft();

		m_pPopup->MoveComponent(ptPOP);
		m_pPopup->ShowComponent(TRUE);

		m_bPopupVisible = TRUE;
	}
}
BOOL CTGuildMercenaryNewDlg::ShowPopupCastle( WORD wCastleID )
{
	if( wCastleID <= 0 )
		return FALSE;

	if( m_nLastSelType != RINFO_NAME )
		return FALSE;
	
	CTClientChar* pMainChar = CTClientGame::GetInstance()->GetMainChar();
	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();

	size_t index = ( wCastleID / TCASTLE_COUNT ) - 1;
	const CTGuildCommander::Territory &Terr = pCmd->GetTerritory( index );

	WORD wMyCastle = 0;
		GuildTactics* pInfo = pCmd->GetTacticsPtr( m_nLastSelIdx );
		wMyCastle = pInfo->m_wCastle;

	if( pMainChar->m_bGuildDuty == GUILD_DUTY_CHIEF && pMainChar->m_dwTacticsID == 0 &&
		(pMainChar->m_strGUILD == Terr.m_strDefGuild || pMainChar->m_strGUILD == Terr.m_strAtkGuild) &&
		Terr.m_bCanApplyCastle == TRUE )
	{
		if( wMyCastle != wCastleID )
			return TRUE;
	}

	return FALSE;
}
// ----------------------------------------------------------------------
void CTGuildMercenaryNewDlg::CancelPopup()
{
	RemoveKid(m_pPopup);
	m_pPopup->ClearButtons();
	m_bPopupVisible = FALSE;
}
// ----------------------------------------------------------------------
BOOL CTGuildMercenaryNewDlg::IsPopupVisible() const
{
	return m_bPopupVisible;
}

int CTGuildMercenaryNewDlg::OnGM_GMP_WHISPER2()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();

	CString strCHAR;
		GuildTactics* pInfo = pCmd->GetTacticsPtr(m_nLastSelIdx);
		strCHAR = pInfo->m_strName;

	CTClientGame* pGame = CTClientGame::GetInstance();
	CTChatFrame* pChat = pGame->GetChatFrame();
	pChat->SetChatMode(CHAT_WHISPER, strCHAR);

	if( !pChat->IsChatEditON() )
		pGame->EnableChat(TRUE);

	return TERR_NONE;
}		
// ----------------------------------------------------------------------
int CTGuildMercenaryNewDlg::OnGM_GMP_MAIL()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTClientGame* pGame = CTClientGame::GetInstance();
	if( !pGame->CanUseMail() )
		return TERR_MAIL_REGION;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();

	CString strCHAR;
		GuildTactics* pInfo = pCmd->GetTacticsPtr(m_nLastSelIdx);
		strCHAR = pInfo->m_strName;

	CTMailSendDlg* pSndDlg = static_cast<CTMailSendDlg*>(pGame->GetFrame(TFRAME_MAIL_SEND));
	pSndDlg->SetReceiver(strCHAR);
	pSndDlg->SetTitle("");

	pGame->EnableUI(TFRAME_MAIL_SEND);

	return TERR_NONE;
}
// ----------------------------------------------------------------------
int CTGuildMercenaryNewDlg::OnGM_GMP_PARTY()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	CTClientGame* pGame = CTClientGame::GetInstance();
	CTClientChar* pMainChar = pGame->GetMainChar();

	BYTE bObtainType;
	if( pGame->GetPartyID(pMainChar) )
		bObtainType = pGame->GetPartyItemType();
	else
		bObtainType = PT_ORDER;

		GuildTactics* pInfo = pCmd->GetTacticsPtr( m_nLastSelIdx );
		pGame->GetSession()->SendCS_PARTYADD_REQ( pInfo->m_strName, bObtainType);

	return TERR_NONE;
}
// ----------------------------------------------------------------------
int CTGuildMercenaryNewDlg::OnGM_GMP_FRIEND()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();

		GuildTactics* pInfo = pCmd->GetTacticsPtr( m_nLastSelIdx );
		CTMessengerCommander::GetInstance()->DoAddFriend( pInfo->m_strName );

	return TERR_NONE;
}
// ----------------------------------------------------------------------
int CTGuildMercenaryNewDlg::OnGM_GMP_DISORG_TACTICS()
{
	CancelPopup();

	/// 미구현

	return TERR_NONE;
}
// ----------------------------------------------------------------------
int CTGuildMercenaryNewDlg::OnGM_GMP_CANCEL()
{
	CancelPopup();

	return TERR_NONE;
}

/*void CTGuildMercenaryNewDlg::EnableGuildUI( BOOL bEnable )
{
	m_bGuildEnable = bEnable;
}*/

int CTGuildMercenaryNewDlg::OnGM_GMP_VOLUNTEER_HESED()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	CTClientGame* pGame = CTClientGame::GetInstance();

		GuildTactics* info = pCmd->GetTacticsPtr(m_nLastSelIdx);
		info->m_wCastle = TCASTLE_HESED;
		pGame->GetSession()->SendCS_CASTLEAPPLY_REQ( info->m_wCastle, info->m_dwCharID );

//	ResetInfo();

	return TERR_NONE;
}

int CTGuildMercenaryNewDlg::OnGM_GMP_VOLUNTEER_ARDRI()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	CTClientGame* pGame = CTClientGame::GetInstance();

		GuildTactics* info = pCmd->GetTacticsPtr(m_nLastSelIdx);
		info->m_wCastle = TCASTLE_ARDRI;
		pGame->GetSession()->SendCS_CASTLEAPPLY_REQ( info->m_wCastle, info->m_dwCharID );

//	ResetInfo();

	return TERR_NONE;
}

int CTGuildMercenaryNewDlg::OnGM_GMP_VOLUNTEER_TYCON()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	CTClientGame* pGame = CTClientGame::GetInstance();

		GuildTactics* info = pCmd->GetTacticsPtr(m_nLastSelIdx);
		info->m_wCastle = TCASTLE_TYCON;
		pGame->GetSession()->SendCS_CASTLEAPPLY_REQ( info->m_wCastle, info->m_dwCharID );

//	ResetInfo();

	return TERR_NONE;
}

int CTGuildMercenaryNewDlg::OnGM_GMP_VOLUNTEER_GEHBRA()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	CTClientGame* pGame = CTClientGame::GetInstance();

		GuildTactics* info = pCmd->GetTacticsPtr(m_nLastSelIdx);
		info->m_wCastle = TCASTLE_GEHBRA;
		pGame->GetSession()->SendCS_CASTLEAPPLY_REQ( info->m_wCastle, info->m_dwCharID );

//	ResetInfo();

	return TERR_NONE;
}

int CTGuildMercenaryNewDlg::OnGM_GMP_VOLUNTEER_CANCEL()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	CTClientGame* pGame = CTClientGame::GetInstance();

		GuildTactics* info = pCmd->GetTacticsPtr(m_nLastSelIdx);
		pGame->GetSession()->SendCS_CASTLEAPPLY_REQ( info->m_wCastle, info->m_dwCharID );

//	ResetInfo();

	return TERR_NONE;
}

int CTGuildMercenaryNewDlg::OnGM_GMP_TACTICS_RE_CONTRACT()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	GuildTactics* pInfo = pCmd->GetTacticsPtr(m_nLastSelIdx);

	CTClientGame* pGAME = CTClientGame::GetInstance();

	if( !pInfo->m_strName.IsEmpty() )
	{
		CTTacticsInviteDlg* pDLG = static_cast<CTTacticsInviteDlg*>( pGAME->GetFrame( TFRAME_INVITE_TACTICS ) );
		pDLG->SetReInviteCond( pInfo->m_strName );
		pGAME->EnableUI( TFRAME_INVITE_TACTICS );
	}

	return TERR_NONE;
}

int CTGuildMercenaryNewDlg::OnGM_GMP_TACTICS_KICK()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	GuildTactics* pInfo = pCmd->GetTacticsPtr(m_nLastSelIdx);

	pCmd->RequestGuildTacticsKickOut(
		pInfo->m_strName,
		pInfo->m_dwCharID,
		1 );

	return TERR_NONE;
}

int CTGuildMercenaryNewDlg::OnGM_GMP_TACTICS_SELF_KICK()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	GuildTactics* pInfo = pCmd->GetTacticsPtr(m_nLastSelIdx);

	pCmd->RequestGuildTacticsKickOut(
		pInfo->m_strName,
		pInfo->m_dwCharID,
		2 );

	return TERR_NONE;
}

int CTGuildMercenaryNewDlg::OnGM_GMP_TACTICS_REWARD_POINT()
{
	CancelPopup();

	if(  m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildPointRewardFrame* pDLG = (CTGuildPointRewardFrame*)
		CTClientGame::GetInstance()->GetFrame( TFRAME_GUILDPOINTREWARD );

	pDLG->m_pGuildPoint->m_strText.Format( "%d",
		CTGuildCommander::GetInstance()->GetGuildDetInfo().m_dwPvPUseablePoint );

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	
	pDLG->m_pTargetEdit->ClearText();
	pDLG->m_pPointEdit->ClearText();
	pDLG->m_pMessage->ClearText();

	pDLG->m_pTargetEdit->SetText( pCmd->GetTacticsPtr( m_nLastSelIdx )->m_strName );

	pDLG->SwitchFocus( pDLG->m_pTargetEdit );
	pDLG->m_pTargetEdit->SetFocus(TRUE);

	CTClientGame::GetInstance()->EnableUI( TFRAME_GUILDPOINTREWARD );

	return TERR_NONE;
}

HRESULT CTGuildMercenaryNewDlg::Render( DWORD dwTickCount)
{
	/*RemoveKid(m_pListC);
	AddKid(m_pListC);
	
	m_pListC->ShowComponent();
	m_pListC->EnableComponent();

	if( IsPopupVisible() )
		{
			RemoveKid( m_pPopup );
			AddKid( m_pPopup );
		}*/

	/*HRESULT hr = ITInnerFrame::Render(dwTickCount);

	return hr;*/
	return ITInnerFrame::Render( dwTickCount );
}


