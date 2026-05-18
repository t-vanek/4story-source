#include "stdafx.h"
#include "BRPannels.h"
#include "BRTeamDlg.h"
#include "TClientGame.h"
#include "TCustomStrings.h"

CBRPannels::CBRPannels(TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc)
: CTClientUIBase(pParent, pDesc)
{
	TFrame* pTeamDLG = new CTClientUIBase(this, CTClientGame::GetInstance()->m_pTParser->FindFrameTemplate(1302));
	CPoint pPos[COMP_COUNT] = { }; 

	m_pButton[0] = static_cast<TButton*>(FindKid(47407));

	m_pDangerPH[0] = new TComponent(this, pTeamDLG->FindKid(26622)->m_pDESC);
	CPoint ptButton;
	m_pButton[0]->GetComponentPos(&ptButton);
	m_pDangerPH[0]->MoveComponent(ptButton);

	m_pDangerPH[0]->GetComponentPos(&pPos[COMP_DANGER_PH]);
	m_pDangerPH[0]->m_id = GetUniqueID();
	m_pDangerPH[0]->SetStyle(TS_CUSTOM_COLOR);
	m_pDangerPH[0]->m_dwColor = 0xFFFF0A0A;
	m_pDangerPH[0]->m_bEnable = FALSE;
	AddKid(m_pDangerPH[0]);

	m_pGauge[0] = static_cast<TGauge*>(FindKid(26281));
	AddKid(m_pGauge[0]);

	m_pHeartPH[0] = FindKid(26355);
	AddKid(m_pHeartPH[0]);

	m_pLifes[0] = FindKid(26354);
	AddKid(m_pLifes[0]);

	m_pName[0] = FindKid(26405);
	AddKid(m_pName[0]);

	m_pDanger[0] = new TComponent(this, m_pLifes[0]->m_pDESC);

	m_pGauge[0]->GetComponentPos(&pPos[COMP_GAUGE]);
	m_pHeartPH[0]->GetComponentPos(&pPos[COMP_HEARTPH]);
	m_pLifes[0]->GetComponentPos(&pPos[COMP_LIFES]);
	m_pName[0]->GetComponentPos(&pPos[COMP_NAME]);
	m_pButton[0]->GetComponentPos(&pPos[COMP_BUTTON]);

	m_pDanger[0]->MoveComponentBy(20, 0);
	m_pDanger[0]->GetComponentPos(&pPos[COMP_DANGER]);
	m_pDanger[0]->m_id = GetUniqueID();
	m_pDanger[0]->m_strText.Empty();
	AddKid(m_pDanger[0]);

	for(BYTE i = 1; i < BR_TEAMMATE_MAX_COUNT(BR_3V3); ++i)
	{
		m_pGauge[i] = new TGauge(this, m_pGauge[0]->m_pDESC);
		m_pGauge[i]->m_id = GetUniqueID();

		m_pHeartPH[i] = new TComponent(this, m_pHeartPH[0]->m_pDESC);
		m_pHeartPH[i]->m_id = GetUniqueID();

		m_pLifes[i] = new TComponent(this, m_pLifes[0]->m_pDESC);
		m_pLifes[i]->m_id = GetUniqueID();

		m_pName[i] = new TComponent(this, m_pName[0]->m_pDESC);
		m_pName[i]->m_id = GetUniqueID();

		m_pButton[i] = new TButton(this, m_pButton[0]->m_pDESC);
		m_pButton[i]->m_id = GetUniqueID();

		m_pDanger[i] = new TComponent(this, m_pDanger[0]->m_pDESC);
		m_pDanger[i]->m_id = GetUniqueID();
		m_pDanger[i]->m_strText.Empty();

		m_pDangerPH[i] = new TComponent(this, m_pDangerPH[0]->m_pDESC);
		m_pDangerPH[i]->m_id = GetUniqueID();
		m_pDangerPH[i]->SetStyle(TS_CUSTOM_COLOR);
		m_pDangerPH[i]->m_dwColor = 0xFFFF0000;

		for(BYTE l = 0; l < COMP_COUNT; ++l)
			pPos[l].y += m_pGauge[0]->m_rc.Height() + 12;

		m_pGauge[i]->MoveComponent(pPos[COMP_GAUGE]);
		m_pHeartPH[i]->MoveComponent(pPos[COMP_HEARTPH]);
		m_pLifes[i]->MoveComponent(pPos[COMP_LIFES]);
		m_pName[i]->MoveComponent(pPos[COMP_NAME]);
		m_pButton[i]->MoveComponent(pPos[COMP_BUTTON]);
		m_pDanger[i]->MoveComponent(pPos[COMP_DANGER]);
		m_pDangerPH[i]->MoveComponent(pPos[COMP_DANGER_PH]);

		AddKid(m_pButton[i]);
		AddKid(m_pDangerPH[i]);
		AddKid(m_pGauge[i]);
		AddKid(m_pHeartPH[i]);
		AddKid(m_pLifes[i]);
		AddKid(m_pName[i]);
		AddKid(m_pDanger[i]);

		m_pDangerPH[i]->m_bEnable = FALSE;
	}

	for(BYTE i = 0; i < BR_TEAMMATE_MAX_COUNT(BR_3V3); ++i) 
		m_pGauge[i]->SetStyle(TGS_GROW_RIGHT);

	m_bCanDrag = TRUE;
	m_rcDrag.SetRect(
		0, 0,
		CTachyonRes::m_pDEVICE->m_option.m_dwScreenX,
		CTachyonRes::m_pDEVICE->m_option.m_dwScreenY);

	m_nLastSel = T_INVALID;
}

BOOL CBRPannels::CanWithItemUI()
{
	return TRUE;
}

CBRPannels::~CBRPannels()
{

}

void CBRPannels::ShowComponent(BOOL bVisible)
{
	if (bVisible)
	{
		for (BYTE i = 0; i < BR_TEAMMATE_MAX_COUNT(BR_3V3); ++i)
		{
			m_pHeartPH[i]->m_pEnable = NULL;
			m_pHeartPH[i]->UseOwnImages(25003);
		}
	}

	return CTClientUIBase::ShowComponent(bVisible);
}

void CBRPannels::UpdatePannels()
{
	CTClientGame* pClientGame = CTClientGame::GetInstance();
	BOOL bRefused = FALSE;

	if(!pClientGame)
		return;

	VBRTEAM::iterator it;
	for(it = pClientGame->m_vBRTEAM.begin(); it != pClientGame->m_vBRTEAM.end(); ++it)
	{
		INT nIndex = (*it)->m_nIndex;
		if (nIndex == T_INVALID || nIndex > BR_TEAMMATE_MAX_COUNT(BR_3V3) - 1)
			continue;

		CTClientChar* pCHAR = pClientGame->FindPC((*it)->m_dwCharID);
		BOOL Dead = (*it)->m_bDead;
		bRefused = !pCHAR;
		
		if (Dead || bRefused)
			m_pDanger[nIndex]->m_strText.Empty();

		if(bRefused)
		{
			m_pGauge[nIndex]->SetGauge(0, 1, FALSE);
			m_pLifes[nIndex]->m_strText.Format("%d", (*it)->m_bLife);
			m_pName [nIndex]->m_strText.Format(!Dead ? "%s" : "Dead (%s)", (*it)->m_strName);
			RemoveKid(m_pHeartPH[nIndex]);
			m_pHeartPH[nIndex]->ShowComponent(FALSE);
		}
		else
		{
			m_pGauge[nIndex]->SetGauge(Dead ? 0 : pCHAR->m_dwHP, pCHAR->m_dwMaxHP, FALSE);
			m_pLifes[nIndex]->m_strText.Format("%d", (*it)->m_bLife);
			m_pName [nIndex]->m_strText.Format(!Dead ? "%s" : "Dead (%s)", (*it)->m_strName);
			if (!Dead)
			{
				if (!m_pHeartPH[nIndex]->IsVisible())
					AddKid(m_pHeartPH[nIndex]);
				m_pHeartPH[nIndex]->ShowComponent(TRUE);
			}
			else
			{
				m_pHeartPH[nIndex]->ShowComponent(FALSE);
				RemoveKid(m_pHeartPH[nIndex]);
			}
		}
	}
}

void CBRPannels::OnLButtonUp(UINT nFlags, CPoint pt)
{
	CTClientGame* pClientGame = CTClientGame::GetInstance();
	if(!pClientGame)
		return;

	for(BYTE i = 0; i < BR_TEAMMATE_MAX_COUNT(BR_3V3); ++i)
	{
		if(m_pGauge[i]->HitTest(pt) || m_pButton[i]->HitTest(pt))
		{
			VBRTEAM::iterator it;
			for(it = pClientGame->m_vBRTEAM.begin(); it != pClientGame->m_vBRTEAM.end(); ++it) 
			{
				if((*it)->m_nIndex == i)
				{
					CTClientObjBase::m_mapTSELECTOBJ.clear();
					CTClientObjBase::m_mapTSELECTDEADOBJ.clear();

					if(!pClientGame->FindPC((*it)->m_dwCharID))
						return;

					pClientGame->ResetTargetOBJ(pClientGame->FindPC((*it)->m_dwCharID));
					pClientGame->ResetTargetINFO(FALSE);
					break;
				}
			}
		}
	}

	CTClientUIBase::OnLButtonUp(nFlags,pt);
}

void CBRPannels::OnRButtonUp(UINT nFlags, CPoint pt)
{
	CTClientGame* pClientGame = CTClientGame::GetInstance();
	for (BYTE i = 0; i < BR_TEAMMATE_MAX_COUNT(BR_3V3); ++i)
	{
		if (m_pGauge[i]->HitTest(pt) || m_pButton[i]->HitTest(pt))
		{
			VBRTEAM::iterator it;
			for(it = pClientGame->m_vBRTEAM.begin(); it != pClientGame->m_vBRTEAM.end(); ++it) 
			{
				if ((*it)->m_nIndex == i && pClientGame->GetMainChar()->m_dwID != (*it)->m_dwCharID)
				{
					m_nLastSel = i;
					BuildMenu();

					break;
				}
			}
		}
	}

	//CTClientUIBase::OnRButtonDown(nFlags, pt);
}

HRESULT CBRPannels::Render(DWORD dwTickCount)
{
	if (!IsVisible())
		return S_OK;

	for (BYTE i = 0; i < BR_TEAMMATE_MAX_COUNT(BR_3V3); ++i)
	{
		if (!m_pDangerPH[i]->m_bEnable)
		{
			m_pDangerPH[i]->ShowComponent(FALSE);
			continue;
		}

		m_pDangerPH[i]->ShowComponent(m_pDangerPH[i]->m_bFocus);
	}

	static DWORD dwBlinkTick = 0;
	dwBlinkTick += dwTickCount;
	if (dwBlinkTick >= 500)
	{
		for (BYTE i = 0; i < BR_TEAMMATE_MAX_COUNT(BR_3V3); ++i)
		{
			if (!m_pDangerPH[i]->m_bEnable)
				continue;

			m_pDangerPH[i]->m_bFocus = !m_pDangerPH[i]->m_bFocus;
		}

		dwBlinkTick = 0;
	}

	for (BYTE i = 0; i < BR_TEAMMATE_MAX_COUNT(BR_3V3); ++i)
	{
		FLOAT HPPercent = 0;
		HPPercent = (FLOAT) m_pGauge[i]->GetCurValue() / m_pGauge[i]->GetMaxValue();
		BYTE bRed = 0;
		bRed = 127 - (BYTE) ((FLOAT) HPPercent * 127.0f);

		if (HPPercent >= 0.50f) {
			m_pDangerPH[i]->m_bEnable = FALSE;
		}

		if (HPPercent > 0.70f)
		{
			m_pDanger[i]->m_strText.Empty();
			m_pDangerPH[i]->m_bEnable = FALSE;
		}
		else if (HPPercent < 0.30f)
		{
			m_pDanger[i]->m_strText = "!!!";
			m_pDanger[i]->SetTextClr(Color::MakeARGB(255, bRed + 127, 0, 0));
			m_pDangerPH[i]->m_bEnable = TRUE;
		}
		else if (HPPercent < 0.50f)
		{
			m_pDanger[i]->m_strText = "!!";
			m_pDanger[i]->SetTextClr(Color::MakeARGB(255, bRed + 127, 0, 0));
			m_pDangerPH[i]->m_bEnable = TRUE;
		}
		else if(HPPercent < 0.70f)
		{
			m_pDanger[i]->m_strText = "!";
			m_pDanger[i]->SetTextClr(Color::MakeARGB(255, bRed + 127, 0, 0));
		}

		if (!m_pGauge[i]->GetCurValue())
		{
			m_pDangerPH[i]->m_bEnable = FALSE;
			m_pDanger[i]->m_strText.Empty();
		}
	}

	if (CTClientGame::GetInstance()->m_vBRTEAM.size() == BR_TEAMMATE_MAX_COUNT(BR_2V2))
	{
		m_pButton[2]->ShowComponent(FALSE);
		m_pGauge[2]->ShowComponent(FALSE);
		m_pLifes[2]->ShowComponent(FALSE);
		m_pName[2]->ShowComponent(FALSE);
		if (m_pHeartPH[2]->IsVisible())
			RemoveKid(m_pHeartPH[2]);
		m_pHeartPH[2]->ShowComponent(FALSE);
	}
	else if (CTClientGame::GetInstance()->m_vBRTEAM.size() == BR_TEAMMATE_MAX_COUNT(BR_3V3))
	{
		m_pButton[2]->ShowComponent(TRUE);
		m_pGauge[2]->ShowComponent(TRUE);
		m_pLifes[2]->ShowComponent(TRUE);
		m_pName[2]->ShowComponent(TRUE);
		m_pHeartPH[2]->ShowComponent(TRUE);
	}

	return CTClientUIBase::Render(dwTickCount);
}

void CBRPannels::BuildMenu()
{
	CTClientGame* pTGAME = CTClientGame::GetInstance();

	static DWORD dwMenuID[][2] = {
		{ GM_DONATE_LIFE, 3007},
		{ GM_CLOSE_POPUP, TSTR_CANCEL}};

	for(BYTE i = 0; i < 2; i++)
		pTGAME->m_pTPOPUP->AddMENU( dwMenuID[i][1], dwMenuID[i][0]);

	pTGAME->ShowPOPUP();
}