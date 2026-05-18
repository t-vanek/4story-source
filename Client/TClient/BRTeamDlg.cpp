#include "stdafx.h"
#include "BRTeamDlg.h"
#include "TClientGame.h"
#include "TClientWnd.h"

#define MAP_COUNT 4

enum TeamComponents 
{
	TEAMCOMP_BUTTON = 0,
	TEAMCOMP_GAUGE,
	TEAMCOMP_MARKER,
	TEAMCOMP_NAME,
	TEAMCOMP_COUNT
};

CBRTeamDlg::CBRTeamDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
: CTClientUIBase( pParent, pDesc )
{
	CPoint Point[TEAMCOMP_COUNT];

	m_pTeamButton[0] = (TButton*) FindKid(47407);
	m_pTeamGauge[0] = (TGauge*) FindKid(26281);
	m_pInTeamMarker[0] = FindKid(26622);
	m_pTeamName[0] = FindKid(26288);
	m_pTeamInvite[0] = (TButton*) FindKid(27956);
	m_pTeamLeave[0] = (TButton*) FindKid(27957);

	m_pTeamChief = FindKid(26295);

	AddKid(m_pTeamButton[0]);
	AddKid(m_pInTeamMarker[0]);
	AddKid(m_pTeamGauge[0]);
	AddKid(m_pTeamName[0]);
	AddKid(m_pTeamChief);

	m_pPlaceHolder = FindKid(11665);
	m_pText = FindKid(16);
	m_pText->MoveComponentBy(0, 5);

	m_pStart = (TButton*) FindKid(26891);
	m_pStart->GetComponentPos(&Point[TEAMCOMP_BUTTON]); Point[TEAMCOMP_BUTTON].y += 2;
	m_pStart->MoveComponent(Point[TEAMCOMP_BUTTON]);
	m_pStart->m_strText = "Join";

	m_pMapList = (TButton*) FindKid(26094);
	m_pMapList->m_strText = "Please vote for Map";
	m_pMapList->SetPushButton();

	m_pModeList = new TButton(this, m_pMapList->m_pDESC);
	m_pModeList->m_id = GetUniqueID();
	m_pModeList->MoveComponentBy(0, 28);
	AddKid(m_pModeList);

	m_pList = static_cast<TList*>(FindKid(ID_CTRLINST_TITLE_GROUPLIST));
	m_pList2 = new TList(this, m_pList->m_pDESC);
	m_pList2->m_id = GetUniqueID();
	AddKid(m_pList2);

	CPoint ptList;
	m_pMapList->GetComponentPos(&ptList);
	ptList.y += 24;
	m_pList->MoveComponent(ptList);
	m_pList2->MoveComponent(CPoint(ptList.x, ptList.y + 28));

	m_pTeamButton[0]->GetComponentPos(&Point[TEAMCOMP_BUTTON]);
	m_pTeamGauge[0]->GetComponentPos(&Point[TEAMCOMP_GAUGE]);
	m_pInTeamMarker[0]->GetComponentPos(&Point[TEAMCOMP_MARKER]);
	m_pTeamName[0]->GetComponentPos(&Point[TEAMCOMP_NAME]);

	m_pTeamGauge[0]->SetStyle(TGS_GROW_RIGHT);
	m_pTeamGauge[0]->SetGauge(10, 10, TRUE);

	for (BYTE i = 0; i < TIME_IMAGE_COUNT; ++i)
		m_pTime[i] = (TImageList*) FindKid(26971 - i);

	std::vector<CString> vMap;
	static CString strMap[MAP_COUNT] = 
	{
		"Blonea",
		"Hod",
		"Tyconteroga",
		"Colossus"
	};

	for (BYTE i = 0; i < MAP_COUNT; ++i)
		vMap.push_back(strMap[i]);

	LoadMapList(vMap);

	for (BYTE i = 1; i < BR_TEAM_MEMBER; ++i)
	{
		m_pTeamButton[i] = new TButton(this, m_pTeamButton[0]->m_pDESC);
		m_pTeamGauge[i] = new TGauge(this, m_pTeamGauge[0]->m_pDESC);
		m_pInTeamMarker[i] = new TComponent(this, m_pInTeamMarker[0]->m_pDESC);
		m_pTeamName[i] = new TComponent(this, m_pTeamName[0]->m_pDESC);

		m_pTeamButton[i]->m_id = GetUniqueID();
		m_pTeamGauge[i]->m_id = GetUniqueID();
		m_pInTeamMarker[i]->m_id = GetUniqueID();
		m_pTeamName[i]->m_id = GetUniqueID();

		for (BYTE p = 0; p < TEAMCOMP_COUNT; ++p)
			Point[p].y += 27;

		m_pTeamButton[i]->MoveComponent(Point[TEAMCOMP_BUTTON]);
		m_pTeamGauge[i]->MoveComponent(Point[TEAMCOMP_GAUGE]);
		m_pInTeamMarker[i]->MoveComponent(Point[TEAMCOMP_MARKER]);
		m_pTeamName[i]->MoveComponent(Point[TEAMCOMP_NAME]);

		m_pTeamGauge[i]->SetStyle(TGS_GROW_RIGHT);

		AddKid(m_pTeamButton[i]);
		AddKid(m_pInTeamMarker[i]);
		AddKid(m_pTeamGauge[i]);
		AddKid(m_pTeamName[i]);

		m_pTeamName[i]->m_strText.Format("(Player %d)", i + 1);
	}

	m_pTeamInvite[1] = new TButton(this, m_pTeamInvite[0]->m_pDESC);
	m_pTeamLeave[1] = new TButton(this, m_pTeamLeave[0]->m_pDESC);

	m_pTeamInvite[1]->m_id = GetUniqueID();
	m_pTeamLeave[1]->m_id = GetUniqueID();

	for (BYTE i = 0; i < BR_TEAM_MEMBER - 1; ++i)
	{
		m_pTeamInvite[i]->MoveComponent(CPoint(
			m_pTeamInvite[i]->m_rc.left, 
			m_pTeamInvite[i]->m_rc.top + (i + 1) * 27));

		m_pTeamLeave[i]->MoveComponent(CPoint(
			m_pTeamLeave[i]->m_rc.left, 
			m_pTeamLeave[i]->m_rc.top + (i + 1) * 27));

		AddKid(m_pTeamInvite[i]);
		AddKid(m_pTeamLeave[i]);
	}

	for (BYTE i = 0; i < 4; ++i)
		m_pTime[i]->SetCurImage(0);

	m_pPlaceHolder->m_pEnable = NULL;
	m_pPlaceHolder->MoveComponentBy(-8, -5);

	AddKid(m_pList);

	m_bMapListVisible = FALSE;
	m_bModeListVisible = FALSE;
	m_strSelectedMap.Empty();
	LeaveTeam();

	m_style |= TS_CUSTOM_COLOR;
	m_dwColor = 0xAA000000;

	for (auto& Kids : m_kids)
	{
		if (Kids->m_strText != NAME_NULL || Kids->m_bType == TCML_TYPE_BUTTON || Kids->m_id == m_pTeamChief->m_id || Kids->m_pDESC.lock() == m_pInTeamMarker[0]->m_pDESC.lock())
			continue;

		Kids->m_style |= TS_CUSTOM_COLOR;
		Kids->m_dwColor = 0xAAFFFFFF;

		if (Kids->m_bType == TCML_TYPE_IMAGELIST)
		{
			auto ImageList = (TImageList*)Kids;
			ImageList->EnableUserColor(TRUE);
			ImageList->SetUserColor(0xDDFFFFFF);
		}
	}
}

CBRTeamDlg::~CBRTeamDlg()
{
	
}

BOOL CBRTeamDlg::CanWithItemUI()
{
	return TRUE;
}

void CBRTeamDlg::ShowComponent(BOOL bVisible)
{
	if (m_bTeamReady && !bVisible)
		return;

	m_pPlaceHolder->UseOwnImages(1302);

	return CTClientUIBase::ShowComponent(bVisible);
}

void CBRTeamDlg::SetTime(DWORD dwTime)
{
	if (!m_bTeamReady)
		return;

	BYTE bMin = (INT) dwTime / 60;
	BYTE bSec = (INT) dwTime % 60;

	BYTE bTime[4] = { 0 };
	bTime[0] = bMin / 10;
	bTime[1] = bMin % 10;
	bTime[2] = bSec / 10;
	bTime[3] = bSec % 10;

	for (BYTE i = 0; i < 4; ++i)
		m_pTime[i]->SetCurImage(bTime[i]);
}

void CBRTeamDlg::LoadMapList(const std::vector<CString>& vMaps)
{
	for (BYTE i = 0; i < vMaps.size(); ++i)
	{
		int nLine = m_pList->AddString(vMaps[i]);
		m_pList->SetItemData(nLine, 0, i);
	}

	int nLine = m_pList2->AddString("3 Players");
	m_pList2->SetItemData(nLine, 0, 0);

	nLine = m_pList2->AddString("2 Players");
	m_pList2->SetItemData(nLine, 0, 1);
}

void CBRTeamDlg::LeaveTeam()
{
	m_bTeamReady = FALSE;
	m_bIsChief = FALSE;
	m_strSelectedMap.Empty();
	m_vTeam.clear();
	m_Mode = T_INVALID;
}

void CBRTeamDlg::UpdateTeam(std::vector<BRPREMADEPLAYER>& vTeam, BYTE bTeamReady)
{
	m_bTeamReady = bTeamReady;
	m_vTeam = vTeam;
}

HRESULT CBRTeamDlg::Render(DWORD dwTickCount)
{
	m_pList->ShowComponent(m_bMapListVisible);
	m_pList2->ShowComponent(m_bModeListVisible);

	static DWORD dwOriClr = m_pText->m_pFont->m_dwColor;
	static CString strOriText = m_pText->m_strText;
	static DWORD dwChangeTick = GetTickCount();
	static BYTE bTeamReady = FALSE;

	if (!m_strSelectedMap.IsEmpty())
		m_pMapList->m_strText = m_strSelectedMap;
	else
		m_pMapList->m_strText = "Please vote for Map";

	if (m_Mode == T_INVALID)
		m_pModeList->m_strText = "Please vote for Mode";
	else
	{
		switch (m_Mode)
		{
		case BR_3V3:
			m_pModeList->m_strText = "3 Players";
			break;
		case BR_2V2:
			m_pModeList->m_strText = "2 Players";
			break;
		default:
			m_pModeList->m_strText = "Please vote for Mode";
			break;
		}
	}

	if (bTeamReady != m_bTeamReady)
	{
		dwChangeTick = 0;
		bTeamReady = m_bTeamReady;
	}

	if (GetTickCount() - dwChangeTick >= 7500)
	{
		if (m_pText->m_strText == strOriText && !m_vTeam.empty())
		{
			CString strText = NAME_NULL;
			DWORD dwColor = 0xFFE80E0E;
			if (!m_bTeamReady)
			{
				dwColor = 0xFFE80E0E;
				strText.Format("Your team is not ready! You won't be able to participate the battle, unless everyone in your team will be ready. When your team will be ready, the timer will start counting down.");
			}
			else
			{
				dwColor = 0xFF1CF50C;
				strText.Format("Your team is ready. You will soon be able to participate the battle, but please keep in mind, that there is not 100%% chance that you will get into the battle.");
			}

			m_pText->m_strText = strText;
			m_pText->SetTextClr(dwColor);
		}
		else
		{
			m_pText->m_strText = strOriText;
			m_pText->SetTextClr(dwOriClr);
		}

		dwChangeTick = GetTickCount();
	}

	CString strMainName = NAME_NULL;
	if (m_vTeam.empty())
	{
		m_pTeamName[0]->m_strText = CTClientGame::GetInstance()->GetMainChar()->m_strNAME;
		m_pStart->m_strText = "Join";
		for (BYTE i = 1; i < BR_TEAM_MEMBER; ++i)
		{
			m_pTeamGauge[i]->SetGauge(0, 1, FALSE);
			m_pTeamName[i]->m_strText.Format("(Player %d)", i + 1);
			m_pInTeamMarker[i]->ShowComponent(FALSE);

			m_pTeamLeave[i - 1]->ShowComponent(FALSE);
			m_pTeamInvite[i - 1]->ShowComponent(TRUE);
		}

		return CTClientUIBase::Render(dwTickCount);
	}
	else
	{
		strMainName = CTClientGame::GetInstance()->GetMainChar()->m_strNAME;
		m_bIsChief = m_vTeam[0].m_strName == strMainName;
		if (!m_bIsChief)
			m_pStart->m_strText = "Ready";
		else
			m_pStart->m_strText = "Join";
	}

	for (BYTE i = 0; i < BR_TEAM_MEMBER; ++i)
	{
		if (i < m_vTeam.size())
		{
			m_pTeamGauge[i]->SetGauge(10, 10, TRUE);
			m_pTeamName[i]->m_strText = m_vTeam[i].m_strName;

			if (m_vTeam[i].m_bReady)
				m_pInTeamMarker[i]->ShowComponent(TRUE);
			else
				m_pInTeamMarker[i]->ShowComponent(FALSE);

			m_pTeamGauge[i]->ShowComponent(TRUE);
			m_pTeamName[i]->ShowComponent(TRUE);

			if (i > 0)
				m_pTeamInvite[i - 1]->ShowComponent(FALSE);

			if (m_bIsChief && i > 0 || m_vTeam[i].m_strName == strMainName && i > 0)
				m_pTeamLeave[i - 1]->ShowComponent(TRUE);
			else if (i > 0)
				m_pTeamLeave[i - 1]->ShowComponent(FALSE);
		}
		else
		{
			m_pTeamGauge[i]->SetGauge(0, 1, FALSE);
			m_pTeamName[i]->m_strText.Format("(Player %d)", i + 1);
			m_pInTeamMarker[i]->ShowComponent(FALSE);

			if (i > 0)
				m_pTeamLeave[i - 1]->ShowComponent(FALSE);

			if (m_bIsChief && i > 0)
				m_pTeamInvite[i - 1]->ShowComponent(TRUE);
			else if (i > 0)
				m_pTeamInvite[i - 1]->ShowComponent(FALSE);
		}
	}

	return CTClientUIBase::Render(dwTickCount);
}

void CBRTeamDlg::OnLButtonUp(UINT nFlags, CPoint pt)
{
	if (!IsVisible())
		return CTClientUIBase::OnLButtonUp(nFlags, pt);

	for (BYTE i = 0; i < BR_TEAM_MEMBER - 1; ++i)
	{
		if (m_pTeamInvite[i]->HitTest(pt) && m_pTeamInvite[i]->IsVisible())
		{
			CTClientGame* pTGAME = CTClientGame::GetInstance();
			pTGAME->GetMainWnd()->MessageBox(
				"Please enter name of player you would like to add to your team.",
				"Name:",
				0,
				TSTR_YES,
				TSTR_NO,
				ID_FRAME_2BTNMSGBOX,
				GM_BRTEAMMATEADD_REQ,
				GM_CLOSE_MSGBOX_WITH_RESETCONMODE,
				GM_CLOSE_MSGBOX_WITH_RESETCONMODE,
				TRUE,
				TRUE);

			break;
		}

		if (m_pTeamLeave[i]->HitTest(pt) && m_pTeamLeave[i]->IsVisible())
		{
			CTClientGame* pTGAME = CTClientGame::GetInstance();
			if (pTGAME->GetSession() && !m_pTeamName[i + 1]->m_strText.IsEmpty())
				pTGAME->GetSession()->SendCS_BRTEAMMATEDEL_REQ(m_pTeamName[i + 1]->m_strText);

			break;
		}
	}

	if (m_pStart->HitTest(pt) && m_pStart->IsVisible())
	{
		CTClientGame* pTGAME = CTClientGame::GetInstance();
		if (pTGAME->GetSession())
		{
			if (m_strSelectedMap.IsEmpty() || (m_Mode != BR_3V3 && m_Mode != BR_2V2))
			{
				pTGAME->GetMainWnd()->MessageBoxOK(
					"Please vote for first!",
					TSTR_OK, 
					GM_CLOSE_MSGBOX, 
					GM_CLOSE_MSGBOX);

				return CTClientUIBase::OnLButtonUp(nFlags, pt);
			}

			pTGAME->GetSession()->SendCS_REGISTERBR_REQ(!m_vTeam.empty());
		}
	}

	if (m_pMapList->HitTest(pt) && m_pMapList->IsVisible() || 
		m_bMapListVisible && !m_pList->HitTest(pt))
	{
		m_bMapListVisible = !m_bMapListVisible;
	}

	if(m_pList->HitTest(pt))
	{
		TListItem* pMAP = m_pList->GetHitItem(pt);
		if (pMAP)
		{
			CTClientGame* pTGAME = CTClientGame::GetInstance();
			if (pTGAME->GetSession())
			{
				m_strSelectedMap = pMAP->m_strText;
				pTGAME->GetSession()->SendCS_VOTEFORBRMAP_REQ(pMAP->m_strText, T_INVALID);
				m_bMapListVisible = FALSE;

				return CTClientUIBase::OnLButtonUp(nFlags, pt);
			}
		}
	}

	if (m_pModeList->HitTest(pt) && m_pModeList->IsVisible() ||
		m_bModeListVisible && !m_pList2->HitTest(pt))
	{
		m_bModeListVisible = !m_bModeListVisible;
	}

	if (m_pList2->HitTest(pt))
	{
		TListItem* Mode = m_pList2->GetHitItem(pt);
		if (Mode)
		{
			CTClientGame* pTGAME = CTClientGame::GetInstance();
			if (pTGAME->GetSession())
			{
				m_Mode = (BYTE) Mode->m_param;
				pTGAME->GetSession()->SendCS_VOTEFORBRMAP_REQ(NAME_NULL, m_Mode);
				m_bModeListVisible = FALSE;

				return CTClientUIBase::OnLButtonUp(nFlags, pt);
			}
		}
	}

	return CTClientUIBase::OnLButtonUp(nFlags, pt);
}