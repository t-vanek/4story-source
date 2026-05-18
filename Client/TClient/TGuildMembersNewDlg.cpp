#include "StdAfx.h"
#include "TFameRank.h"
#include "TGuildMembersNewDlg.h"
#include "TClientGame.h"
#include "Resource.h"
#include "TClient.h"
#include "TGuildPointRewardFrame.h"
#include "TMailSendDlg.h"
#include "TTacticsInviteDlg.h"
BYTE CTGuildMembersNewDlg::m_bTabIndex = TGUILD_MEMBERS;

const CTGuildMembersNewDlg::PopupBtnInfo CTGuildMembersNewDlg::m_PopupBtnInfo[TGM_PBTN_COUNT] = 
{
	{TSTR_GMP_WHISPER,				GM_GMP_WHISPER			},	///< TGM_PBTN_WHISPER,		
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
	{TSTR_GMP_VOLUNTEER_WOODLAND,	GM_GMP_VOLUNTEER_WOODLAND	},
	{TSTR_GMP_VOLUNTEER_CANCEL,		GM_GMP_VOLUNTEER_CANCEL	},

	{TSTR_GMP_TACTICS_RE_CONTRACT,	GM_GMP_TACTICS_RE_CONTRACT	}, ///< TGM_PBTN_TACTICS_RE_CONTRACT,			///< 용병 재계약
	{TSTR_GMP_TACTICS_KICK,			GM_GMP_TACTICS_KICK			}, ///< TGM_PBTN_TACTICS_KICK,						///< 용병 계약 파기
	{TSTR_GMP_TACTICS_REWARD_POINT,	GM_GMP_TACTICS_REWARD_POINT	}, ///< TGM_PBTN_TACTICS_REWARD_POINT,			///< 용병 포상
	{TSTR_GMP_TACTICS_SELF_KICK,		GM_GMP_TACTICS_SELF_KICK } //TGM_PBTN_TACTICS_SELF_KICK,					///< 용병 탈퇴
};


CTGuildMembersNewDlg::CTGuildMembersNewDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc, FRAMEDESC_SHAREDPTR pPopupDesc)
	: ITInnerFrame(pParent, pDesc, TGUILD_MEMBERS),
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

CTGuildMembersNewDlg::~CTGuildMembersNewDlg()
{
	if( m_pPopup )
		delete m_pPopup;
}

void CTGuildMembersNewDlg::RequestInfo()
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
			pSession->SendCS_GUILDMEMBERLIST_REQ();
			pSession->SendCS_GUILDPVPRECORD_REQ();
		}
}

void CTGuildMembersNewDlg::ResetInfo()
{
	CTGuildCommander* pGuildCmd = CTGuildCommander::GetInstance();

	pGuildCmd->SortGuildMember();

			int nTop = m_pListC->GetTop();

			//ResetInfo_Member();
			m_pListC->RemoveAll();

			CTGuildCommander::GuildMemberVec::iterator itMEMBER, end;
			itMEMBER = pGuildCmd->m_GuildMemberVec.begin();
			end = pGuildCmd->m_GuildMemberVec.end();

			CTGuildCommander* pTGUILDCMDER = CTGuildCommander::GetInstance();

			for(; itMEMBER != end ; ++itMEMBER )
			{
				int nLine = m_pListC->AddString("");

				DWORD dwColor = 0;
				switch( itMEMBER->m_wCastle )
				{
				case TCASTLE_HESED : dwColor = COLOR_CASTLE_HESED; break;
				case TCASTLE_ARDRI : dwColor = COLOR_CASTLE_ARDRI; break;
				case TCASTLE_GEHBRA : dwColor = COLOR_CASTLE_GEHBRA; break;
				case TCASTLE_TYCON : dwColor = COLOR_CASTLE_TYCON; break;
				case TCASTLE_WOODLAND : dwColor = COLOR_CASTLE_WOODLAND; break;
				default : dwColor = itMEMBER->m_dwTactics ? COLOR_TACTICS : COLOR_CASTLE_NONE; break;
				}

				for(int i=0; i<RINFO_COUNT; ++i)
				{
					switch( i )
					{
					case RINFO_NAME:
						{
							m_pListC->SetItemString( nLine, 1, itMEMBER->m_strName );
							m_pListC->SetItemData( nLine, 1, MAKELONG( nLine, RINFO_NAME) );
							m_pListC->SetUserColor( nLine, 1, dwColor );
							m_pListC->SetColumnAlign( 1, ALIGN_CENTER );
						}
						break;
					
					case RINFO_CLASS:
						{
							/*m_pListC->SetItemString( nLine, 2,
								CTChart::LoadString( (TSTRING) CTClientGame::m_vTCLASSSTR[ itMEMBER->m_bClassID] ) );*/

							m_pListC->SetItemData( nLine, 2, MAKELONG( nLine, RINFO_CLASS) );
							m_pListC->SetImageIndex( nLine, 2, TRUE, itMEMBER->m_bClassID);
							m_pListC->SetUserColor( nLine, 2, dwColor );
							m_pListC->SetColumnAlign( 2, ALIGN_CENTER );
						}
						break;

					case RINFO_LEVEL	:
						{
							m_pListC->SetItemString( nLine, 3,
								CTChart::Format( TSTR_FMT_NUMBER, itMEMBER->m_bLevel) );

							m_pListC->SetItemData( nLine, 3, MAKELONG( nLine, RINFO_LEVEL) );
							m_pListC->SetUserColor( nLine, 3, dwColor );
							m_pListC->SetColumnAlign( 3, ALIGN_CENTER );
						}
						break;

					case RINFO_SERVICE:
						{
							CString strPEER;
							if( itMEMBER->m_dwTactics )
							{
								strPEER = CTChart::LoadString( TSTR_GUILD_PEER_TACTICS );
							}
							else
							{
								strPEER = CTGuildCommander::GetPeerageStr( itMEMBER->m_bPeer );
							}

							m_pListC->SetItemString( nLine, 4, strPEER );

							m_pListC->SetItemData( nLine, 4, MAKELONG( nLine, RINFO_PEER) );
							m_pListC->SetUserColor( nLine, 4, dwColor );
							m_pListC->SetColumnAlign( 4, ALIGN_CENTER );
						}
						break;

					case RINFO_DUTY:
						{
							CTGuildCommander* pTGUILDCMDER = CTGuildCommander::GetInstance();
							CTGuildCommander::GuildLatestPvP* pDayPvp = pTGUILDCMDER->GetGuildLatestPvPbyID(itMEMBER->m_dwCharID);
							CTGuildCommander::GuildWeekPvP* pWeekPvp = pTGUILDCMDER->GetGuildWeekPvPbyID(itMEMBER->m_dwCharID);

							DWORD dwDay = pDayPvp ? pDayPvp->m_wKillCount_D : 0;
							DWORD dwWeek = pWeekPvp ? pWeekPvp->m_wKillCount_W : 0;

							CString strRes;
							strRes.Format("%d/%d",dwDay,dwWeek);
							m_pListC->SetItemString( nLine, 5, strRes);

							m_pListC->SetItemData( nLine, 5, MAKELONG( nLine, RINFO_DUTY) );
							m_pListC->SetUserColor( nLine, 5, dwColor );
							m_pListC->SetColumnAlign( 5, ALIGN_CENTER );
						}
						break;

					case RINFO_PEER:
						{
							if( itMEMBER->m_bIsConnect )
							{
								LPTREGIONINFO pRegion = CTChart::FindTREGIONINFO( itMEMBER->m_dwRegion);

								if( pRegion )
									m_pListC->SetItemString( nLine, 6, pRegion->m_strNAME);
							}
							else
							{
								CTime time(itMEMBER->m_dlConnectedDate);

								CString strFMT;
								strFMT.Format(" %d.%d.%d - %02d::%02d",
									time.GetYear(),
									time.GetMonth(),
									time.GetDay(),
									time.GetHour(),
									time.GetMinute() );

								m_pListC->SetItemString( nLine, 6, strFMT );
							}

							m_pListC->SetItemData( nLine, 6, MAKELONG( nLine, RINFO_POSITION) );
							m_pListC->SetUserColor( nLine, 6, dwColor );
							m_pListC->SetColumnAlign( 6, ALIGN_CENTER );
						}
						break;	
					}
				}
			}

			RemoveKid( m_pListC );
			AddKid( m_pListC );

			//m_pListC->SetTopSelItem( nTop );
}

HRESULT CTGuildMembersNewDlg::Render( DWORD dwTickCount )
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

	return ITInnerFrame::Render( dwTickCount );

}
void CTGuildMembersNewDlg::OnLButtonDown( UINT nFlags, CPoint pt )
{
	if( !IsPopupVisible() || !m_pPopup->HitTest(pt) )
	{
		m_pDownItem = m_pListC->GetHitItem( pt);
	}

	ITInnerFrame::OnLButtonDown( nFlags, pt );
}

void CTGuildMembersNewDlg::OnLButtonUp( UINT nFlags, CPoint pt )
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

void CTGuildMembersNewDlg::ShowPopup(const CPoint& pt)
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
				if( m_nLastSelType == RINFO_NAME )
				{
					const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
					if( info.m_bIsConnect || i == TGM_PBTN_MAIL )
					{
						if( pMainChar->m_strNAME != info.m_strName )
							bADD = TRUE;
					}
				}
			}
			break;

		case TGM_PBTN_POINT_REWARD:
			{
					if( m_nLastSelType == RINFO_NAME )
					{
						if( pMainChar->m_bGuildDuty == GUILD_DUTY_CHIEF && pMainChar->m_dwTacticsID == 0 )
						{
							bADD = TRUE;
						}
					}
			}
			break;

		case TGM_PBTN_PARTY			:
			if( m_nLastSelType == RINFO_NAME )
			{
					const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
					if ( info.m_bIsConnect && pMainChar->m_strNAME != info.m_strName )
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
					const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
					if ( info.m_bIsConnect && pMainChar->m_strNAME != info.m_strName )
					{
						bADD = TRUE;
					}
			}
			break;

		case TGM_PBTN_KICK			:
		case TGM_PBTN_SET_GM		:	
		case TGM_PBTN_SET_GSM		:	
		case TGM_PBTN_CANCEL_GSM	:	
			/*if( m_nLastSelType == RINFO_DUTY )
			{
				const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);

				if( pMainChar->m_strNAME != info.m_strName &&
					pMainChar->m_bGuildDuty == GUILD_DUTY_CHIEF && pMainChar->m_dwTacticsID == 0 )
				{
					bADD = TRUE;
				}
			}
			break;*/
			if( m_nLastSelType == RINFO_NAME )
			{
				const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);

				if( pMainChar->m_strNAME != info.m_strName )
				{
					if( pMainChar->m_bGuildDuty == GUILD_DUTY_CHIEF && pMainChar->m_dwTacticsID == 0 )
						bADD = TRUE;
					else if( pMainChar->m_bGuildDuty == GUILD_DUTY_VICECHIEF && pMainChar->m_dwTacticsID == 0 )
					{
						if( info.m_strName != detif.m_strGMaster )
							bADD = TRUE;
					}
				}
			}
			break;

		case TGM_PBTN_PEER_BARON	:	
		case TGM_PBTN_PEER_VISCOUNT	:	
		case TGM_PBTN_PEER_COUNT	:	
		case TGM_PBTN_PEER_MARQUIS	:
		case TGM_PBTN_PEER_DUKE		:	
		case TGM_PBTN_CANCEL_PEER	:	
			if( m_nLastSelType == RINFO_PEER )
			{
				const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);

				if( pMainChar->m_bGuildDuty == GUILD_DUTY_CHIEF && pMainChar->m_dwTacticsID == 0 )
				{
					bADD = TRUE;
				}
			}
			break;
		case TGM_PBTN_VOLUNTEER_CANCEL	:
			if( pMainChar->m_bGuildDuty == GUILD_DUTY_CHIEF && pMainChar->m_dwTacticsID == 0 && m_nLastSelType == RINFO_NAME )
			{
					const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
					if( info.m_wCastle )
						bADD = TRUE;
			}
			break;
		case TGM_PBTN_VOLUNTEER_HESED : bADD = ShowPopupCastle( TCASTLE_HESED ); break;
		case TGM_PBTN_VOLUNTEER_ARDRI : bADD = ShowPopupCastle( TCASTLE_ARDRI ); break;
		case TGM_PBTN_VOLUNTEER_GEHBRA : bADD = ShowPopupCastle( TCASTLE_GEHBRA ); break;
		case TGM_PBTN_VOLUNTEER_TYCON : bADD = ShowPopupCastle( TCASTLE_TYCON ); break;

		//case TGM_PBTN_TACTICS_RE_CONTRACT: ///< 용병 재계약
		//	if( m_nLastSelType == RINFO_NAME )
		//	{
		//		if( pMainChar->m_bGuildDuty == GUILD_DUTY_CHIEF && pMainChar->m_dwTacticsID == 0 )
		//		{
		//			bADD = TRUE;
		//		}
		//	}
		//	break;

		//case TGM_PBTN_TACTICS_KICK: ///< 용병 계약 파기
		//	if( m_nLastSelType == RINFO_NAME )
		//	{
		//		if( pMainChar->m_bGuildDuty == GUILD_DUTY_CHIEF && pMainChar->m_dwTacticsID == 0 )
		//		{
		//			bADD = TRUE;
		//		}
		//	}
		//	break;

		/*case TGM_PBTN_TACTICS_REWARD_POINT: ///< 용병 포상
			if( m_nCurPage == TPVPM_TACTICS && m_nLastSelType == RINFO_NAME )
			{
				if( pMainChar->m_bGuildDuty == GUILD_DUTY_CHIEF && pMainChar->m_dwTacticsID == 0 )
				{
					bADD = TRUE;
				}
			}
			break;*/

		case TGM_PBTN_TACTICS_SELF_KICK:
			{
				
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
BOOL CTGuildMembersNewDlg::ShowPopupCastle( WORD wCastleID )
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
		const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
		wMyCastle = info.m_wCastle;

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
void CTGuildMembersNewDlg::CancelPopup()
{
	RemoveKid(m_pPopup);
	m_pPopup->ClearButtons();
	m_bPopupVisible = FALSE;
}
// ----------------------------------------------------------------------
BOOL CTGuildMembersNewDlg::IsPopupVisible() const
{
	return m_bPopupVisible;
}

int CTGuildMembersNewDlg::OnGM_GMP_WHISPER()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();

	CString strCHAR;
		const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
		strCHAR = info.m_strName;

	CTClientGame* pGame = CTClientGame::GetInstance();
	CTChatFrame* pChat = pGame->GetChatFrame();
	pChat->SetChatMode(CHAT_WHISPER, strCHAR);

	if( !pChat->IsChatEditON() )
		pGame->EnableChat(TRUE);

	return TERR_NONE;
}		
// ----------------------------------------------------------------------
int CTGuildMembersNewDlg::OnGM_GMP_MAIL()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTClientGame* pGame = CTClientGame::GetInstance();
	if( !pGame->CanUseMail() )
		return TERR_MAIL_REGION;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();

	CString strCHAR;
		const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
		strCHAR = info.m_strName;

	CTMailSendDlg* pSndDlg = static_cast<CTMailSendDlg*>(pGame->GetFrame(TFRAME_MAIL_SEND));
	pSndDlg->SetReceiver(strCHAR);
	pSndDlg->SetTitle("");

	pGame->EnableUI(TFRAME_MAIL_SEND);

	return TERR_NONE;
}
// ----------------------------------------------------------------------
int CTGuildMembersNewDlg::OnGM_GMP_PARTY()
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

		const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
		pGame->GetSession()->SendCS_PARTYADD_REQ( info.m_strName, bObtainType);

	return TERR_NONE;
}
// ----------------------------------------------------------------------
int CTGuildMembersNewDlg::OnGM_GMP_FRIEND()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();

		const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
		CTMessengerCommander::GetInstance()->DoAddFriend( info.m_strName );

	return TERR_NONE;
}
// ----------------------------------------------------------------------
int CTGuildMembersNewDlg::OnGM_GMP_KICK()
{
	CancelPopup();

	if(  m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
	pCmd->RequestGuildKickOut(info.m_strName, TRUE);

	return TERR_NONE;
}
// ----------------------------------------------------------------------
int CTGuildMembersNewDlg::OnGM_GMP_DISORG_TACTICS()
{
	CancelPopup();

	/// 미구현

	return TERR_NONE;
}
// ----------------------------------------------------------------------
int CTGuildMembersNewDlg::OnGM_GMP_ET_GM()
{
	CancelPopup();

	if(  m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
	pCmd->RequestGuildDuty(info.m_strName, GUILD_DUTY_CHIEF);

	return TERR_NONE;
}	
// ----------------------------------------------------------------------
int CTGuildMembersNewDlg::OnGM_GMP_SET_GSM()
{
	CancelPopup();

	if(  m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
	pCmd->RequestGuildDuty(info.m_strName, GUILD_DUTY_VICECHIEF);

	return TERR_NONE;
}	
// ----------------------------------------------------------------------
int CTGuildMembersNewDlg::OnGM_GMP_CANCEL_GSM()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
	pCmd->RequestGuildDuty(info.m_strName, GUILD_DUTY_NONE);

	return TERR_NONE;
}
// ----------------------------------------------------------------------
int CTGuildMembersNewDlg::OnGM_GMP_PEER_BARON()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
	pCmd->RequestGuildPeer(info.m_strName, GUILD_PEER_BARON);

	return TERR_NONE;
}
// ----------------------------------------------------------------------
int CTGuildMembersNewDlg::OnGM_GMP_PEER_VISCOUNT()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
	pCmd->RequestGuildPeer(info.m_strName, GUILD_PEER_VISCOUNT);

	return TERR_NONE;
}
// ----------------------------------------------------------------------
int CTGuildMembersNewDlg::OnGM_GMP_PEER_COUNT()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
	pCmd->RequestGuildPeer(info.m_strName, GUILD_PEER_COUNT);

	return TERR_NONE;
}
// ----------------------------------------------------------------------
int CTGuildMembersNewDlg::OnGM_GMP_PEER_MARQUIS()
{
	CancelPopup();

	if(  m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
	pCmd->RequestGuildPeer(info.m_strName, GUILD_PEER_MARQUIS);

	return TERR_NONE;
}
// ----------------------------------------------------------------------
int CTGuildMembersNewDlg::OnGM_GMP_PEER_DUKE()
{
	CancelPopup();

	if(  m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
	pCmd->RequestGuildPeer(info.m_strName, GUILD_PEER_DUKE);

	return TERR_NONE;
}
// ----------------------------------------------------------------------
int CTGuildMembersNewDlg::OnGM_GMP_POINT_REWARD()
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

	pDLG->m_pTargetEdit->SetText( pCmd->GetGuildMember( m_nLastSelIdx ).m_strName );

	pDLG->SwitchFocus( pDLG->m_pTargetEdit );
	pDLG->m_pTargetEdit->SetFocus(TRUE);

	CTClientGame::GetInstance()->EnableUI( TFRAME_GUILDPOINTREWARD );

	return TERR_NONE;
}
// ----------------------------------------------------------------------
int CTGuildMembersNewDlg::OnGM_GMP_CANCEL_PEER()
{
	CancelPopup();

	if(  m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	const GuildMember& info = pCmd->GetGuildMember(m_nLastSelIdx);
	pCmd->RequestGuildPeer(info.m_strName, GUILD_PEER_NONE);

	return TERR_NONE;
}
// ----------------------------------------------------------------------
int CTGuildMembersNewDlg::OnGM_GMP_CANCEL()
{
	CancelPopup();

	return TERR_NONE;
}

/*void CTGuildMembersNewDlg::EnableGuildUI( BOOL bEnable )
{
	m_bGuildEnable = bEnable;
}*/

int CTGuildMembersNewDlg::OnGM_GMP_VOLUNTEER_HESED()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	CTClientGame* pGame = CTClientGame::GetInstance();

		GuildMember* info = pCmd->GetGuildMemberPtr(m_nLastSelIdx);
		info->m_wCastle = TCASTLE_HESED;
		pGame->GetSession()->SendCS_CASTLEAPPLY_REQ( info->m_wCastle, info->m_dwCharID );

//	ResetInfo();

	return TERR_NONE;
}

int CTGuildMembersNewDlg::OnGM_GMP_VOLUNTEER_ARDRI()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	CTClientGame* pGame = CTClientGame::GetInstance();

		GuildMember* info = pCmd->GetGuildMemberPtr(m_nLastSelIdx);
		info->m_wCastle = TCASTLE_ARDRI;
		pGame->GetSession()->SendCS_CASTLEAPPLY_REQ( info->m_wCastle, info->m_dwCharID );

//	ResetInfo();

	return TERR_NONE;
}

int CTGuildMembersNewDlg::OnGM_GMP_VOLUNTEER_TYCON()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	CTClientGame* pGame = CTClientGame::GetInstance();

		GuildMember* info = pCmd->GetGuildMemberPtr(m_nLastSelIdx);
		info->m_wCastle = TCASTLE_TYCON;
		pGame->GetSession()->SendCS_CASTLEAPPLY_REQ( info->m_wCastle, info->m_dwCharID );

//	ResetInfo();

	return TERR_NONE;
}

int CTGuildMembersNewDlg::OnGM_GMP_VOLUNTEER_GEHBRA()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	CTClientGame* pGame = CTClientGame::GetInstance();

		GuildMember* info = pCmd->GetGuildMemberPtr(m_nLastSelIdx);
		info->m_wCastle = TCASTLE_GEHBRA;
		pGame->GetSession()->SendCS_CASTLEAPPLY_REQ( info->m_wCastle, info->m_dwCharID );

//	ResetInfo();

	return TERR_NONE;
}

int CTGuildMembersNewDlg::OnGM_GMP_VOLUNTEER_CANCEL()
{
	CancelPopup();

	if( m_nLastSelIdx == T_INVALID )
		return TERR_NONE;

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	CTClientGame* pGame = CTClientGame::GetInstance();

		GuildMember* info = pCmd->GetGuildMemberPtr(m_nLastSelIdx);
		pGame->GetSession()->SendCS_CASTLEAPPLY_REQ( info->m_wCastle, info->m_dwCharID );

//	ResetInfo();

	return TERR_NONE;
}

int CTGuildMembersNewDlg::OnGM_GMP_TACTICS_RE_CONTRACT()
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

int CTGuildMembersNewDlg::OnGM_GMP_TACTICS_KICK()
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

int CTGuildMembersNewDlg::OnGM_GMP_TACTICS_SELF_KICK()
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

int CTGuildMembersNewDlg::OnGM_GMP_TACTICS_REWARD_POINT()
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


