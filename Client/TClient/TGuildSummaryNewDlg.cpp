#include "StdAfx.h"
#include "TFameRank.h"
#include "TGuildSummaryNewDlg.h"
#include "TClientGame.h"
#include "Resource.h"
#include "TClient.h"
BYTE CTGuildSummaryNewDlg::m_bTabIndex = TGUILD_SUMMARY;

// ====================================================================
const BUTTON_STATE CTGuildSummaryNewDlg::MODE_BTN_VIEW[MODE_COUNT][BUTTON_COUNT] =
{
	//MODE_NORMAL,
	{	
		{0, 0, 0, 0 },							
		{0,	0, 0, 0 },
		{0,	0, 0, 0 },
	},								
	
	//MODE_MASTER,
	{ 
		{1, 1, GM_NEW_NOTIFY, TSTR_NEW_NOTIFY },
		{1,	1, GM_MOD_NOTIFY, TSTR_MODIFY},
		{1,	1, GM_DELETE_NOTIFY, TSTR_DELETE},
	},		
	
	//MODE_NEW,	
	{ 
		{1,	1, GM_NEW_NOTIFY_OK, TSTR_OK },
		{1, 1, GM_NEW_NOTIFY_CANCEL, TSTR_CANCEL },		
		{0,	0, 0, 0},
	},	
	
	//MODE_EDIT,
	{ 
		{1,	1, GM_MOD_NOTIFY_OK, TSTR_OK },
		{1, 1, GM_MOD_NOTIFY_CANCEL, TSTR_CANCEL },		
		{0,	0, 0, 0} ,
	}, 	
};
// ====================================================================

CTGuildSummaryNewDlg::CTGuildSummaryNewDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc)
	: ITInnerFrame(pParent, pDesc, TGUILD_SUMMARY), m_eMode(MODE_NORMAL), m_nPrvSel(T_INVALID)
{

	static const DWORD dwID[] = 
	{
		ID_CTRLINST_T_GN,	///< 길드명
		ID_CTRLINST_T_GD,	///< 길드 개설 일
		ID_CTRLINST_T_GM,	///< 길드장
		ID_CTRLINST_T_GML,	///< 길드장 작위
		ID_CTRLINST_T_GSM1,	///< 부 길드장
		ID_CTRLINST_T_GSM2,	///< 부 길드장
		ID_CTRLINST_T_GNB,	///< 길드원의 수
		ID_CTRLINST_T_GL,	///< 길드 레벨
		ID_CTRLINST_T_GE,	///< 길드 경험치
		ID_CTRLINST_T_MD,	///< 나의 직위
		ID_CTRLINST_T_ML,	///< 나의 작위
		ID_CTRLINST_RUNE,	///< 길드 자금
		ID_CTRLINST_LUNA,	///< 길드 자금
		ID_CTRLINST_CRON,	///< 길드 자금
		ID_CTRLINST_GN_TEXT,	///< 길드 공지사항
		ID_CTRLINST_T_GR,
		ID_CTRLINST_T_GF,
		ID_CTRLINST_T_GP,
		ID_CTRLINST_MONTH_HONOR,		// INFO_GUILD_MONTH_POINT,
		ID_CTRLINST_T_GR, // INFO_GUILD_RANK_TOTAL,
		ID_CTRLINST_MONTH_RANKING // INFO_GUILD_RANK_MONTH,
	};
	
	for(INT i=0; i<INFO_COUNT; ++i)
	{
		m_vInfoCtrl[i] = FindKid(dwID[i]);

		if( m_vInfoCtrl[i] )
			m_vInfoCtrl[i]->m_strText = "";
	}

	m_pMarkImgset = static_cast<TImageList*>( FindKid(ID_CTRLINST_MARK) );
	m_pBackImgset = static_cast<TImageList*>( FindKid(ID_CTRLINST_MARK_BACK) );
	m_pBackBase = FindKid(ID_CTRLINST_MARK_BASE);

	m_pMarkImgset->EnableUserColor(TRUE);
	m_pBackImgset->EnableUserColor(TRUE);

	m_pGuildMarkButton = (TButton*) FindKid( ID_CTRLINST_BTN_GUILDMARK );
	m_pGuildRewardButton = (TButton*) FindKid( ID_CTRLINST_BTN_POINTREWARDLOG );
	m_pGuildRewardMemberButton = (TButton*) FindKid( 27630 );
	m_pGuildExitButton = (TButton*) FindKid( ID_CTRLINST_BTN_OUT );
	TButton* pBtn = (TButton*) FindKid( ID_CTRLINST_B_GP );
	pBtn->m_menu[ TNM_LCLICK ] = GM_GUILD_DONATE_PVPPOINT;

	pBtn = (TButton*)FindKid(27630);
	pBtn->m_menu[TNM_LCLICK] = GM_GUILD_REWARD_CONTINUE;

	// 임시
	TComponent* pCOMP = FindKid( ID_CTRLINST_18 );
	if( pCOMP )
		pCOMP->MoveComponent( CPoint(pCOMP->m_rc.left+5, pCOMP->m_rc.top) );

	pCOMP = FindKid( ID_CTRLINST_11 );
	if( pCOMP )
		pCOMP->MoveComponent( CPoint(pCOMP->m_rc.left, pCOMP->m_rc.top-9) );


	m_pList = static_cast<TList*>( FindKid(ID_CTRLINST_LIST_NOTIFY) );
	m_pViewContents = static_cast<TList*>( FindKid(ID_CTRLINST_LIST) );

	//m_pViewAuthor = FindKid(ID_CTRLINST_TXT_AUTHOR);
	m_pViewTitle = FindKid(ID_CTRLINST_TXT_TITLE);

	m_pEditTitle = static_cast<TEdit*>( FindKid(ID_CTRLINST_EDIT_TITLE) );
	m_pEditTitle->SetFocus(FALSE);
	
	m_pBTN[0] = FindKid(ID_CTRLINST_BTN_1);
	m_pBTN[1] = FindKid(ID_CTRLINST_BTN_2);
	m_pBTN[2] = FindKid(ID_CTRLINST_BTN_3);

	TComponent* pComp = FindKid(ID_CTRLINST_EDIT);
	RemoveKid(pComp);

	m_pEditContents = new TMultiLineEdit(this, pComp->m_pDESC);
	m_pEditContents->m_id = pComp->m_id;
	AddKid(m_pEditContents);

	delete pComp;
	ShowComponent(FALSE);
}

CTGuildSummaryNewDlg::~CTGuildSummaryNewDlg()
{
	
}
// ====================================================================

void CTGuildSummaryNewDlg::RequestInfo()
{
	CTGuildCommander::GetInstance()
		->RequestGuildDetInfo();
}
// ----------------------------------------------------------------------
void CTGuildSummaryNewDlg::ResetInfo()
{
	CTTextLinker *pTextLinker = CTTextLinker::GetInstance();
	CTClientChar* pMainChar = CTClientGame::GetInstance()->GetMainChar();
	CTGuildCommander* pGuildCmd = CTGuildCommander::GetInstance();
	const CTGuildCommander::GuildDetInfo& info = pGuildCmd->GetGuildDetInfo();

	m_vInfoCtrl[INFO_GNAME]->m_strText = info.m_strGName;

	CTime t( info.m_ldwGDate );
	CString strFMT;
	strFMT.Format( "%d/%d/%d", t.GetYear(), t.GetMonth(), t.GetDay() );
	m_vInfoCtrl[INFO_GDATE]->m_strText = strFMT;

	m_vInfoCtrl[INFO_GMASTER]->m_strText = info.m_strGMaster;
	//m_vInfoCtrl[INFO_GMASTER_PEER]->m_strText = pMainChar->m_dwTacticsID ?
//		CTChart::LoadString( TSTR_GUILD_PEER_TACTICS ) : CTGuildCommander::GetPeerageStr(info.m_bGMasterPeer);

	m_vInfoCtrl[INFO_GSUBMASTER1]->m_strText = info.m_strGSubMaster1;
	m_vInfoCtrl[INFO_GSUBMASTER2]->m_strText = info.m_strGSubMaster2;

	//if( pMainChar->m_dwTacticsID == 0 )
	//	m_vInfoCtrl[INFO_MYDUTY]->m_strText = CTGuildCommander::GetDutyStr(info.m_bMyDuty);
	//else
	//	m_vInfoCtrl[INFO_MYDUTY]->m_strText = CTChart::LoadString( TSTR_FMT_TACTICS );//CRASH

	//m_vInfoCtrl[INFO_MYPEER]->m_strText = CTGuildCommander::GetPeerageStr(info.m_bMyPeerage);//CRASh

	if( info.m_nGMbMax == 0 )
		m_vInfoCtrl[INFO_GMBCNT]->m_strText = CTChart::Format( TSTR_FMT_NUMBER, info.m_nGMbCnt);
	else
		m_vInfoCtrl[INFO_GMBCNT]->m_strText = CTChart::Format( TSTR_FMT_NUMPERNUM, info.m_nGMbCnt, info.m_nGMbMax );

	m_vInfoCtrl[INFO_GLEV]->m_strText = CTChart::Format( TSTR_FMT_NUMBER, info.m_nGLev );
	m_vInfoCtrl[INFO_GEXP]->m_strText = CTChart::Format( TSTR_FMT_NUMPERNUM, info.m_dwGExpCur, info.m_dwGExpMax );
	//m_vInfoCtrl[INFO_MYCONT]->m_strText = CTChart::Format( TSTR_FMT_NUMPERCENT, info.m_bMyService);
	m_vInfoCtrl[INFO_GMONEY_RUNE]->m_strText = CTChart::Format( TSTR_FMT_NUMBER, info.m_nGRune);
	m_vInfoCtrl[INFO_GMONEY_LUNA]->m_strText = CTChart::Format( TSTR_FMT_NUMBER, info.m_nGLuna);
	m_vInfoCtrl[INFO_GMONEY_CRON]->m_strText = CTChart::Format( TSTR_FMT_NUMBER, info.m_nGCron);

	m_vInfoCtrl[INFO_NOTIFY]->ResetTextSetting();
	m_vInfoCtrl[INFO_NOTIFY]->m_strText = pTextLinker->MakeNetToSimpleLinkText(
		m_vInfoCtrl[INFO_NOTIFY],
		info.m_strNotify);

	m_vInfoCtrl[ INFO_GUILD_RANK ]->m_strText.Empty();

	m_vInfoCtrl[ INFO_GUILD_POINT ]->m_strText.Format( "%d",
		info.m_dwPvPTotalPoint );

	m_vInfoCtrl[ INFO_GUILD_USEABLE_POINT ]->m_strText.Format( "%d",
		info.m_dwPvPUseablePoint );

	if( info.m_dwMonthPoint )
        m_vInfoCtrl[ INFO_GUILD_MONTH_POINT ]->m_strText.Format( "%d", info.m_dwMonthPoint );
	else
		m_vInfoCtrl[ INFO_GUILD_MONTH_POINT ]->m_strText.Empty();

	if( info.m_dwRankTotal )
		m_vInfoCtrl[ INFO_GUILD_RANK_TOTAL ]->m_strText.Format( "%d", info.m_dwRankTotal );
	else
		m_vInfoCtrl[ INFO_GUILD_RANK_TOTAL ]->m_strText.Empty();

	if( info.m_dwRankMonth )
		m_vInfoCtrl[ INFO_GUILD_RANK_MONTH ]->m_strText.Format( "%d", info.m_dwRankMonth );
	else
		m_vInfoCtrl[ INFO_GUILD_RANK_MONTH ]->m_strText.Empty();

	if( info.m_bShowMark )
	{
		m_pMarkImgset->ShowComponent(TRUE);
		m_pBackImgset->ShowComponent(TRUE);
		m_pBackBase->ShowComponent(TRUE);

		m_pMarkImgset->SetCurImage( info.m_bMark );
		m_pBackImgset->SetCurImage( info.m_bMarkBack );

		m_pBackBase->m_dwColor = CTClientGuildMark::m_dwCOLOR[info.m_bMarkBackColor1];

		m_pMarkImgset->SetUserColor(CTClientGuildMark::m_dwCOLOR[info.m_bMarkColor]);
		m_pBackImgset->SetUserColor(CTClientGuildMark::m_dwCOLOR[info.m_bMarkBackColor2]);
	}
	else
	{
		m_pMarkImgset->ShowComponent(FALSE);
		m_pBackImgset->ShowComponent(FALSE);
		m_pBackBase->ShowComponent(FALSE);
	}

	if( CTClientGame::GetInstance()->GetMainChar()->m_dwTacticsID != 0 )
		HideComponentWhenTactics();

	/*for(INT i=0; i<BUTTON_COUNT; ++i)
	{
		const BUTTON_STATE& info = MODE_BTN_VIEW[m_eMode][i];

		if(pMainChar->m_bGuildDuty == GUILD_DUTY_CHIEF && pMainChar->m_dwTacticsID == 0)
		{
			m_pBTN[i]->ShowComponent(TRUE);		
			m_pBTN[i]->EnableComponent(TRUE);
			m_pBTN[i]->m_menu[TNM_LCLICK] = info.m_dwBtnGM;

			BOOL bEDIT = IsEditMode();
	
			m_pViewTitle->ShowComponent(!bEDIT);
			m_pEditTitle->ShowComponent(bEDIT);
			m_pViewContents->ShowComponent(!bEDIT);
			m_pEditContents->ShowComponent(bEDIT);

			m_pList->EnableComponent(!bEDIT);

			if( bEDIT )
			{
				m_pEditTitle->ClearText();
				m_pEditContents->ClearText();

				m_pEditTitle->ResetTextSetting();
				m_pEditContents->ResetTextSetting();

				SwitchFocus(m_pEditTitle);
			}
			else
				UpdateByListSel();

		}
		else
			m_pBTN[i]->ShowComponent(FALSE);
	}*/
	ResetMode();
	if(m_pList->GetItemCount() > 0)
	m_pList->SetTopSelItem(0);
}

void CTGuildSummaryNewDlg::HideComponentWhenTactics()
{
	static DWORD dwCOMP[] =
	{
		ID_CTRLINST_16,
		ID_CTRLINST_51,
		ID_CTRLINST_8,
		ID_CTRLINST_17,
		ID_CTRLINST_52,
		ID_CTRLINST_10,
		ID_CTRLINST_T_GR,
		ID_CTRLINST_MONTH_RANKING,
		ID_CTRLINST_T_GNB,
		ID_CTRLINST_T_GF,
		ID_CTRLINST_MONTH_HONOR,
		ID_CTRLINST_T_GL
	};

	for( INT i=0 ; i < 12 ; ++i )
		FindKid( dwCOMP[ i ] )->ShowComponent( FALSE );
}

// ====================================================================
void CTGuildSummaryNewDlg::UpdateInfo()
{
	CTTextLinker *pTextLinker = CTTextLinker::GetInstance();

	m_pList->RemoveAll();
	m_pList->ResetTextSetting();

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	INT nCNT = (INT)pCmd->GetCountGuildNotify();
	for(INT i=0; i<nCNT; ++i)
	{
		const GuildNotify& info = pCmd->GetGuildNotify(i);

		m_pList->AddString(
			pTextLinker->MakeNetToLinkText( NULL, info.m_strTitle ),
			COL_TITLE);

		m_pList->SetItemString(i, COL_DATE, info.m_strDate);
	}

	m_nPrvSel = T_INVALID;

	if( !IsEditMode() )
		UpdateByListSel();
}
// ====================================================================
void CTGuildSummaryNewDlg::ResetMode()
{
	CTClientGame* pGame = CTClientGame::GetInstance();
	CTClientChar* pMainChar = pGame->GetMainChar();

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	const CTGuildCommander::GuildDetInfo& info = pCmd->GetGuildDetInfo();

	if( info.m_strGMaster == pMainChar->m_strNAME )
		m_eMode = MODE_MASTER;
	else
		m_eMode = MODE_NORMAL;

	UpdateCompByMode();
}
// --------------------------------------------------------------------
void CTGuildSummaryNewDlg::UpdateCompByMode()
{
	for(INT i=0; i<BUTTON_COUNT; ++i)
	{
		const BUTTON_STATE& info = MODE_BTN_VIEW[m_eMode][i];

		m_pBTN[i]->ShowComponent(info.m_bShow);
		m_pBTN[i]->EnableComponent(info.m_bEnable);
		m_pBTN[i]->m_menu[TNM_LCLICK] = info.m_dwBtnGM;

		if( info.m_dwBtnTitle )
			m_pBTN[i]->m_strText = CTChart::LoadString( (TSTRING) info.m_dwBtnTitle);
		else
			m_pBTN[i]->m_strText.Empty();

	}
		
	BOOL bEDIT = IsEditMode();
	
	m_pViewTitle->ShowComponent(!bEDIT);
	m_pEditTitle->ShowComponent(bEDIT);
	m_pViewContents->ShowComponent(!bEDIT);
	m_pEditContents->ShowComponent(bEDIT);

	m_pList->EnableComponent(!bEDIT);

	if( bEDIT )
	{
		m_pEditTitle->ClearText();
		m_pEditContents->ClearText();

		m_pEditTitle->ResetTextSetting();
		m_pEditContents->ResetTextSetting();

		SwitchFocus(m_pEditTitle);
	}
	else
		UpdateByListSel();
}
// ====================================================================
void CTGuildSummaryNewDlg::StartNew()
{
	m_eMode = MODE_NEW;

	UpdateCompByMode();
}
// --------------------------------------------------------------------
void CTGuildSummaryNewDlg::EndNew(BOOL bOK)
{
	if( bOK )
	{
		CTTextLinker *pTTextLinker = CTTextLinker::GetInstance();

		CTGuildCommander::GetInstance()->RequestGuildNewNotify(
			pTTextLinker->MakeLinkToNetText( m_pEditTitle, TRUE, MAX_BOARD_TITLE),
			pTTextLinker->MakeLinkToNetText( m_pEditContents, TRUE, MAX_BOARD_TEXT));
	}

	ResetMode();
}
// ====================================================================
void CTGuildSummaryNewDlg::StartModify()
{
	CTTextLinker *pTextLinker = CTTextLinker::GetInstance();

	m_nModIdx = m_pList->GetSel();

	if( m_nModIdx >= 0 )
	{
		m_eMode = MODE_EDIT;
		UpdateCompByMode();

		CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
		const GuildNotify& info = pCmd->GetGuildNotify(m_nModIdx);

		m_pEditTitle->ResetTextSetting();
		m_pEditTitle->SetText(
			pTextLinker->MakeNetToSimpleLinkText(m_pEditTitle, info.m_strTitle) );

		m_pEditContents->ResetTextSetting();
		m_pEditContents->ClearText();
		m_pEditContents->m_strText = pTextLinker->MakeNetToLinkText(m_pEditContents, info.m_strText);
		m_pEditContents->MoveCaretToBack();
	}
}
// --------------------------------------------------------------------
TEdit* CTGuildSummaryNewDlg::GetCurEdit()
{
	if( !IsVisible() || !IsEditMode() )
		return NULL;

	if( m_pEditTitle->CanProcess() && m_pFocus == m_pEditTitle )
		return m_pEditTitle;

	if( m_pEditContents->CanProcess() && m_pFocus == m_pEditContents )
		return m_pEditContents;

	return NULL;
}
// --------------------------------------------------------------------
void CTGuildSummaryNewDlg::EndModify(BOOL bOK)
{
	if( bOK && m_nModIdx >= 0 )
	{
		CTTextLinker *pTTextLinker = CTTextLinker::GetInstance();
		CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
		const GuildNotify& info = pCmd->GetGuildNotify(m_nModIdx);

		pCmd->RequestGuildModNotify(
			info.m_dwID,
			pTTextLinker->MakeLinkToNetText( m_pEditTitle, TRUE, MAX_BOARD_TITLE),
			pTTextLinker->MakeLinkToNetText( m_pEditContents, TRUE, MAX_BOARD_TEXT));
	}

	ResetMode();
}
// ====================================================================
void CTGuildSummaryNewDlg::Delete()
{
	m_nModIdx = m_pList->GetSel();

	if( m_nModIdx >= 0 )
	{
		CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
		const GuildNotify& info = pCmd->GetGuildNotify(m_nModIdx);

		pCmd->RequestGuildDelNotify(info.m_dwID);
	}
}
// ====================================================================
BOOL CTGuildSummaryNewDlg::IsEditMode()
{
	return ( m_eMode == MODE_NEW || m_eMode == MODE_EDIT );
}

// ====================================================================
void CTGuildSummaryNewDlg::UpdateByListSel()
{
	int nSel = m_pList->GetSel();
	if( m_nPrvSel == T_INVALID || m_nPrvSel != nSel )
	{
		if( nSel >= 0 )
		{
			CTTextLinker *pTextLinker = CTTextLinker::GetInstance();

			CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
			const GuildNotify& info = pCmd->GetGuildNotify(nSel);

			//m_pViewAuthor->m_strText = info.m_strAuthor;
			m_pViewTitle->m_strText = pTextLinker->MakeNetToSimpleLinkText( m_pViewTitle, info.m_strTitle); //m_pViewTitle->m_strText = info.m_strTitle;

			m_pViewContents->RemoveAll();
			m_pViewContents->ResetTextSetting();

			TLLINESETTING_ARRAY2D vSETTINGS2D;
			TTLINKLINE_ARRAY vLINES;
			INT nPos = 0;

			CString strLINE = pTextLinker->SplitTextByCrLf( info.m_strText, CTTextLinker::LINK_DEF_TOK, nPos, FALSE);
			while(!strLINE.IsEmpty())
			{
				CString strPMSG = pTextLinker->MakeNetToLinkText( this, strLINE);
				strPMSG.Remove('\r');

				CSize szITEM = m_pViewContents->GetItemSize(0);
				pTextLinker->SplitTextByComp( m_pViewContents, szITEM.cx, strPMSG, vLINES);
				pTextLinker->GetTextSettings( this, vLINES, vSETTINGS2D);

				INT nLineSize = (INT) vLINES.size();
				for( INT i=0; i<nLineSize; i++)
				{
					const TTEXT_LINK_LINE& line = vLINES[i];
					const TLLINESETTING_ARRAY& vLineSettings = vSETTINGS2D[i];

					INT iRow = m_pViewContents->AddString(line.strMSG);
					for( INT k=0; k<INT(vLineSettings.size()); k++)
					{
						const TLINK_LINE_SETTING& s = vLineSettings[k];
						m_pViewContents->AddTextSetting( iRow, 0, s.nStart, s.nEnd, s.dwColor, s.dwInfo);
					}
				}

				vLINES.clear();
				vSETTINGS2D.clear();
				ResetTextSetting();

				strLINE = pTextLinker->SplitTextByCrLf( info.m_strText, CTTextLinker::LINK_DEF_TOK, nPos, FALSE);
			}
		}
		else
		{
			//m_pViewAuthor->m_strText.Empty();
			m_pViewTitle->m_strText.Empty();
			m_pViewTitle->ResetTextSetting();
			m_pViewContents->RemoveAll();
			m_pViewContents->ResetTextSetting();
		}
	}

	m_nPrvSel = nSel;
}
// ====================================================================

// ======================================================================
void CTGuildSummaryNewDlg::OnLButtonDown(UINT nFlags, CPoint pt)
{
	if(!CanProcess()) 
		return;

	if(HitTest(pt))
	{
		CTTextLinker *pTextLinker = CTTextLinker::GetInstance();
		CTClientGame *pTGAME = CTClientGame::GetInstance();
		DWORD dwInfo;

		if( m_pViewContents->HitTestTextSetting(pt, &dwInfo) )
		{
			CTTextLinkData *pLinkData = pTextLinker->GetTextLinkData(dwInfo);

			if( pLinkData && pLinkData->IsItemType() )
			{
				CTClientItem *pItem = pLinkData->GetLastUnkpackItem();

				if(pItem)
					pTGAME->ShowChatDetailInfo(pItem);

				return;
			}
		}

		if( m_pViewTitle->HitTestTextSetting(pt, &dwInfo) )
		{
			CTTextLinkData *pLinkData = pTextLinker->GetTextLinkData(dwInfo);

			if( pLinkData && pLinkData->IsItemType() )
			{
				CTClientItem *pItem = pLinkData->GetLastUnkpackItem();

				if(pItem)
					pTGAME->ShowChatDetailInfo(pItem);

				return;
			}
		}

		SwitchFocus(m_pViewContents);
	}

	if( m_vInfoCtrl[INFO_NOTIFY]->HitTest(pt))
	{
		CTTextLinker *pTextLinker = CTTextLinker::GetInstance();
		CTClientGame *pTGAME = CTClientGame::GetInstance();
		DWORD dwInfo;

		if( m_vInfoCtrl[INFO_NOTIFY]->HitTestTextSetting(pt, &dwInfo) )
		{
			CTTextLinkData *pLinkData = pTextLinker->GetTextLinkData(dwInfo);

			if( pLinkData && pLinkData->IsItemType() )
			{
				CTClientItem *pItem = pLinkData->GetLastUnkpackItem();

				if(pItem)
					pTGAME->ShowChatDetailInfo(pItem);

				return;
			}
		}

		SwitchFocus( m_vInfoCtrl[INFO_NOTIFY]);
	}

	ITInnerFrame::OnLButtonDown( nFlags, pt);
}

// ====================================================================
void CTGuildSummaryNewDlg::OnKeyDown(UINT nChar, int nRepCnt, UINT nFlags)
{
	if(!CanProcess()) 
		return;

	if( nChar == VK_RETURN && m_pEditContents->GetFocus() )
	{
		m_pEditContents->InsertReturnChar();
		m_pEditContents->MoveScrollToCaret(FALSE);
	}
	else
	{
		ITInnerFrame::OnKeyDown(nChar,nRepCnt,nFlags);
	}
}
// ====================================================================

void CTGuildSummaryNewDlg::ShowComponent( BOOL bVisible )
{
	if( m_bVisible == bVisible )
		return;

	CTGuildCommander* pGuildCmd = CTGuildCommander::GetInstance();
	const CTGuildCommander::GuildDetInfo& info = pGuildCmd->GetGuildDetInfo();
	CTClientChar* pMainChar = CTClientGame::GetInstance()->GetMainChar();

	bool bShowGuildExitButton = pMainChar->m_bGuildDuty != GUILD_DUTY_CHIEF || pMainChar->m_dwTacticsID != 0;

	if( bShowGuildExitButton )
	{
		m_pGuildExitButton->m_strText = pMainChar->m_dwTacticsID == 0 ?
			CTChart::LoadString(TSTR_GUILD_BTN_GUILDQUIT) :
			CTChart::LoadString(TSTR_GUILD_BTN_TACTICSQUIT);
	}

	ITInnerFrame::ShowComponent( bVisible );

	m_pGuildMarkButton->ShowComponent( bVisible && pMainChar->m_bGuildDuty == GUILD_DUTY_CHIEF && pMainChar->m_dwTacticsID == 0 );
	m_pGuildRewardButton->ShowComponent(true);
	m_pGuildRewardMemberButton->ShowComponent( bVisible && pMainChar->m_bGuildDuty == GUILD_DUTY_CHIEF && pMainChar->m_dwTacticsID == 0 );
	m_pGuildExitButton->ShowComponent( bVisible && bShowGuildExitButton );
	m_pMarkImgset->ShowComponent(info.m_bShowMark);
	m_pBackImgset->ShowComponent(info.m_bShowMark);
	m_pBackBase->ShowComponent(info.m_bShowMark);

	if( pMainChar->m_dwTacticsID != 0 )
		HideComponentWhenTactics();

		if( bVisible )
			{
				m_bVisible = bVisible;
				ResetMode();

				CTGuildCommander::GetInstance()
					->RequestGuildNotifyList();
			}
}


HRESULT CTGuildSummaryNewDlg::Render( DWORD dwTickCount )
{
	if( IsVisible() )
		UpdateByListSel();

	return ITInnerFrame::Render(dwTickCount);
}

