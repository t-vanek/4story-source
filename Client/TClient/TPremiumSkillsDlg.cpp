#include "stdafx.h"
#include "TPremiumSkillsDlg.h"
#include "TClientGame.h"
#include "TCustomStrings.h"
#include "TClientWnd.h"

#define PREMIUMHOTKEY_FIRST		(THOTKEYBASE_COUNT * MAX_HOTKEY_POS)

CTPremiumSkills::CTPremiumSkills(TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc, CTClientChar* pHost, BYTE bType)
: CTClientUIBase( pParent, pDesc )
{
	static constexpr DWORD dwStartMedals = 27294;
	static constexpr DWORD dwStartHotkey = 27080;
	static constexpr DWORD dwStartTick   = 10802;
	static constexpr DWORD dwStartPress  = 27068;
	static constexpr DWORD dwStartIcon   = 17070;
	static constexpr DWORD dwStartLoop   = 27074;
	
	for (DWORD i = dwStartMedals; i < dwStartMedals + PREMIUM_SKILLS_ONPAGE; ++i)
	{
		m_pMedals[i - dwStartMedals] = FindKid(i);
		m_pMedals[i - dwStartMedals]->m_strText.Empty();
	}
	
	for (DWORD i = dwStartHotkey; i < dwStartHotkey + PREMIUM_SKILLS_ONPAGE; ++i)
	{
		m_pHotkey[i - dwStartHotkey] = FindKid(i);
		m_pHotkey[i - dwStartHotkey]->m_strText.Empty();
	}

	for (DWORD i = dwStartTick; i < dwStartTick + PREMIUM_SKILLS_ONPAGE; ++i)
	{
		m_pTick[i - dwStartTick] = (TGauge*) FindKid(i);
		m_pTick[i - dwStartTick]->SetStyle(TGS_GROW_UP);
		m_pTick[i - dwStartTick]->m_strText.Empty();
	}

	for (DWORD i = dwStartIcon; i < dwStartIcon + PREMIUM_SKILLS_ONPAGE; ++i)
	{
		m_pIcon[i - dwStartIcon] = (TImageList*) FindKid(i);
		m_pIcon[i - dwStartIcon]->SetCurImage(0);
	}

	for (DWORD i = dwStartPress; i < dwStartPress + PREMIUM_SKILLS_ONPAGE; ++i)
	{
		m_pPressed[i - dwStartPress] = (TImageList*) FindKid(i);
		m_pPressed[i - dwStartPress]->SetCurImage(0);
		m_pPressed[i - dwStartPress]->m_bVCenter = FALSE;
	}

	for (DWORD i = dwStartLoop; i < dwStartLoop + PREMIUM_SKILLS_ONPAGE; ++i)
	{
		TComponent* pComponent = FindKid(i);
		if (pComponent)
		{
			pComponent->ShowComponent(FALSE);
			RemoveKid(pComponent);
		}

		delete pComponent;
	}


	m_pHost = pHost;
	m_bType = bType;

	CPoint pt = CTClientUIBase::m_vBasis[TBASISPOINT_CENTER_BOTTOM];
	pt.y -= 186;
	if (!m_bType)
		pt.x -= 343;
	else
		pt.x += 120;

	m_vCompOffset = pt;
	MoveComponent(pt);
}

CTPremiumSkills::~CTPremiumSkills()
{
	m_mapPREMIUMSKILL.clear();
}

BOOL CTPremiumSkills::CanWithItemUI()
{
	return TRUE;
}

void CTPremiumSkills::AddSkill(BYTE bID, WORD wSkillID)
{
	if (bID > PREMIUM_SKILLS)
		return;

	auto itPREMIUMSKILL = m_mapPREMIUMSKILL.find(bID);
	if (itPREMIUMSKILL != m_mapPREMIUMSKILL.end())
		(*itPREMIUMSKILL).second = wSkillID;
	else
		m_mapPREMIUMSKILL.insert(MAPPREMIUMSKILL::value_type(bID, wSkillID));

	auto pCURSKILL = CTChart::FindTSKILLTEMP(wSkillID);
	if (!pCURSKILL || !pCURSKILL->m_wUseMedals)
	{
		m_pMedals[bID]->m_strText.Empty();
		m_pMedals[bID]->ShowComponent(FALSE);
	}
	else
	{
		m_pMedals[bID]->m_strText.Format("%dM", pCURSKILL->m_wUseMedals);
		m_pMedals[bID]->ShowComponent(TRUE);
	}

	ResetHotkeyStr();
}

void CTPremiumSkills::Init()
{
	CTClientGame* pTGAME = CTClientGame::GetInstance();
	if(!pTGAME)
		return;

	MAPPREMIUMSKILL::iterator it;
	for (it = m_mapPREMIUMSKILL.begin(); it != m_mapPREMIUMSKILL.end(); ++it)
	{
		if (!(*it).second && (*it).first < PREMIUM_SKILLS)
		{
			m_pIcon[(*it).first % PREMIUM_SKILLS]->SetCurImage(0);
			m_pPressed[(*it).first % PREMIUM_SKILLS]->ShowComponent(FALSE);
			continue;
		}

		LPTSKILL pCURSKILL = CTChart::FindTSKILLTEMP((*it).second);
		CTClientSkill* pSKILL = m_pHost->FindTSkill((*it).second);

		if (!pCURSKILL)
			continue;

		BYTE bIndex = (*it).first;

		if (!pCURSKILL->m_wUseMedals || m_bType == PREMIUMSKILL_BOW)
			m_pMedals[bIndex]->m_strText.Empty();
		else
			m_pMedals[bIndex]->m_strText.Format("%dM", pCURSKILL->m_wUseMedals);

		if (pSKILL && pSKILL->m_dwTick)
		{
			m_pTick[bIndex]->m_strText = CTClientGame::ToTimeString(pSKILL->m_dwTick);
			m_pTick[bIndex]->SetGauge( pSKILL->m_dwTick, pSKILL->m_dwReuseTick);

			m_pTick[bIndex]->ShowComponent(TRUE);
		}

		m_pIcon[bIndex]->SetCurImage(pCURSKILL->m_wIconID);
	}
}

void CTPremiumSkills::Clear()
{
	for (BYTE i = 0; i < PREMIUM_SKILLS_ONPAGE; ++i)
	{
		m_pIcon[i]->SetCurImage(0);
		m_pTick[i]->ShowComponent(FALSE);
		m_pPressed[i]->ShowComponent(FALSE);
	}//
}

void CTPremiumSkills::ResetHotkeyStr()
{
	CTKeySetting* pKeySet = CTKeySetting::GetInstance();

	for (BYTE i = 0; i < PREMIUM_SKILLS_ONPAGE; ++i)
	{
		WORD wStart = m_bType == 0 ? TKEY_PSKILL1 : TKEY_PSKILL7;
		TKEY_SET eKEY = (TKEY_SET)(wStart + i);

		m_pHotkey[i]->m_strText = pKeySet->VKeyToStr( 
			pKeySet->GetCurVKey(eKEY),
			pKeySet->GetCurModKey(eKEY));
		m_pHotkey[i]->ShowComponent(TRUE);
	}
}

void CTPremiumSkills::CalcHotkeyTick()
{
	CTClientGame* pTGAME = CTClientGame::GetInstance();
	if (!pTGAME)
		return;

	static BOOL InBM = pTGAME->IsInBRMap() || pTGAME->IsInBOWMap();
	BOOL bShouldShow = m_bType == PREMIUMSKILL_BOW ? TRUE : pTGAME->IsInBOWMap() || pTGAME->IsInBRMap();//m_bType == PREMIUMSKILL_BOW ? pTGAME->IsInBOWMap() : TRUE;
	if (IsVisible() != bShouldShow || InBM != pTGAME->IsInBRMap() || InBM != pTGAME->IsInBOWMap())
	{
		if (m_bType == PREMIUMSKILL_BOW)
		{
			if (pTGAME->IsInBRMap() && bShouldShow)
			{
				MAPPREMIUMSKILL::iterator itS6 = m_mapPREMIUMSKILL.find(PREMIUM_SKILLS_ONPAGE - 1);
				if (itS6 != m_mapPREMIUMSKILL.end() && (*itS6).second != 1599)
				{
					(*itS6).second = 1599;

					LPTSKILL pCURSKILL = CTChart::FindTSKILLTEMP((*itS6).second);

					if (pCURSKILL)
						m_pIcon[(*itS6).first]->SetCurImage(pCURSKILL->m_wIconID);

					//Init();
				}
			}
			else if (pTGAME->IsInBOWMap() && bShouldShow)
			{
				MAPPREMIUMSKILL::iterator itS6 = m_mapPREMIUMSKILL.find(PREMIUM_SKILLS_ONPAGE - 1);
				if (itS6 != m_mapPREMIUMSKILL.end() && (*itS6).second != 1507)
				{
					(*itS6).second = 1507;

					LPTSKILL pCURSKILL = CTChart::FindTSKILLTEMP((*itS6).second);

					if (pCURSKILL)
						m_pIcon[(*itS6).first]->SetCurImage(pCURSKILL->m_wIconID);

					//Init();
				}
			}
		}

		InBM = pTGAME->IsInBRMap() || pTGAME->IsInBOWMap();
		ShowComponent(bShouldShow);
	}

	for (BYTE i = 0; i < PREMIUM_SKILLS_ONPAGE; ++i)
	{
		MAPPREMIUMSKILL::iterator itFinder = m_mapPREMIUMSKILL.find(i);
		if (itFinder == m_mapPREMIUMSKILL.end())
		{
			m_pPressed[i]->ShowComponent(FALSE);
			continue;
		}
		else if ((*itFinder).second)
			m_pPressed[i]->ShowComponent(TRUE);
		else
			m_pPressed[i]->ShowComponent(FALSE);

		CTClientSkill *pTSKILL = m_pHost->FindTSkill((*itFinder).second);
		if(pTSKILL && pTSKILL->m_dwTick)
		{
			m_pTick[i]->m_strText = CTClientGame::ToTimeString(pTSKILL->m_dwTick);
			m_pTick[i]->SetGauge(pTSKILL->m_dwTick, pTSKILL->m_dwReuseTick);

			m_pTick[i]->ShowComponent(TRUE);
		}
		else
			m_pTick[i]->ShowComponent(FALSE);

		m_pIcon[i]->EnableComponent(pTSKILL && static_cast<CTGaugePannel*>(CTClientGame::GetInstance()->GetFrame(TFRAME_GAUGE))->IsEnableHotkeySkill(pTSKILL));
	}
}

void CTPremiumSkills::OnLButtonDown( UINT nFlags, CPoint pt )
{
	PushIcon(pt);

	CTClientUIBase::OnLButtonDown( nFlags, pt);
}

void CTPremiumSkills::OnLButtonUp( UINT nFlags, CPoint pt )
{
	ReleaseIcon();

	CTClientUIBase::OnLButtonUp( nFlags, pt);
}

void CTPremiumSkills::OnMouseMove( UINT nFlags, CPoint pt )
{
	INT nX = m_ptPrev.x > pt.x ? m_ptPrev.x - pt.x : pt.x - m_ptPrev.x;
	INT nY = m_ptPrev.y > pt.y ? m_ptPrev.y - pt.y : pt.y - m_ptPrev.y;

	if(nX >= 6 || nY >= 6)
		ReleaseIcon();

	m_ptPrev = pt;
	
	CTClientUIBase::OnMouseMove( nFlags, pt);
}

void CTPremiumSkills::PushIcon(CPoint point)
{
	for (BYTE i = 0; i < PREMIUM_SKILLS_ONPAGE; i++)
	{
		if (!m_pIcon[i]->HitTest(point) || m_mapPREMIUMSKILL.find(i) == m_mapPREMIUMSKILL.end())
			continue;

		CTClientGame::GetInstance()->UsePremiumSkill(m_bType ? PREMIUM_SKILLS + i : i);
		m_pPressed[i]->SetCurImage(TRUE);
		break;
	}
}

void CTPremiumSkills::ReleaseIcon()
{
	for (BYTE i = 0; i < PREMIUM_SKILLS_ONPAGE; i++)
		m_pPressed[i]->SetCurImage(FALSE);
}

ITDetailInfoPtr CTPremiumSkills::GetTInfoKey( const CPoint& point )
{
	BYTE bIndex = (BYTE) T_INVALID;
	for (BYTE i = 0; i < PREMIUM_SKILLS; ++i) {
		if (m_pIcon[i]->HitTest(point))
		{
			bIndex = i;
			break;
		}
	}

	ITDetailInfoPtr pInfo;
	if (bIndex == (BYTE) T_INVALID)
		return pInfo;

	WORD wSkillID = FindSkillID(bIndex);
	if (!wSkillID)
		return pInfo;

	LPTSKILL pTSKILL = CTChart::FindTSKILLTEMP(wSkillID);
	if(pTSKILL)
	{
		CRect rc;
		m_pIcon[bIndex]->GetComponentRect(&rc);
		m_pIcon[bIndex]->ComponentToScreen(&rc);

		CTClientSkill *pTSkill = m_pHost->FindTSkill(wSkillID);
		pInfo = CTDetailInfoManager::NewSkillInst(
			pTSKILL, 
			pTSkill ? pTSkill->m_bLevel : 0,
			FALSE,
			rc);

		pInfo->SetDir(FALSE, TRUE, TRUE);
	}

	return pInfo;
}

WORD CTPremiumSkills::FindSkillID(BYTE bID)
{
	MAPPREMIUMSKILL::iterator finder = m_mapPREMIUMSKILL.find(bID);
	if (finder != m_mapPREMIUMSKILL.end())
		return (*finder).second;

	return 0;
}

void CTPremiumSkills::ResetPosition()
{
	CPoint pt = CTClientUIBase::m_vBasis[TBASISPOINT_CENTER_BOTTOM];
	pt.y -= 186;
	if (!m_bType)
		pt.x -= 343;
	else
		pt.x += 120;

	m_vCompOffset = pt;
	MoveComponent(pt);
}

BYTE CTPremiumSkills::OnBeginDrag(LPTDRAG pDRAG, CPoint point)
{
	if (CTClientGame::IsInBOWMap() || CTClientGame::IsInBRMap() || m_bType != PREMIUMSKILL_BOW)
		return FALSE;

	pDRAG = &CTClientGame::GetInstance()->m_vDRAG;
	pDRAG->m_bFrameID = TFRAME_BOWSKILLS;

	for (BYTE i = 0; i < MAX_HOTKEY_POS / 2; ++i)
	{
		if (m_pIcon[i]->HitTest(point) && m_pIcon[i]->GetCurImage())
		{
			if (pDRAG)
			{
				CPoint pt;
				pDRAG->m_pIMAGE = new TImageList(
					NULL,
					*m_pIcon[i]);

				pDRAG->m_pIMAGE->SetCurImage(m_pIcon[i]->GetCurImage());
				pDRAG->m_pIMAGE->m_strText.Empty();

				pDRAG->m_bSlotID = i;
				pDRAG->m_dwParam = FindSkillID(i);

				m_pIcon[i]->GetComponentPos(&pt);
				m_pIcon[i]->ComponentToScreen(&pt);
				m_pIcon[i]->m_strText.Empty();

				pDRAG->m_pIMAGE->ShowComponent(TRUE);
				pDRAG->m_pIMAGE->MoveComponent(pt);

				return TRUE;
			}
		}
	}

	return FALSE;
}

TDROPINFO CTPremiumSkills::OnDrop(CPoint point)
{
	if (CTClientGame::IsInBOWMap() || CTClientGame::IsInBRMap() || m_bType != PREMIUMSKILL_BOW)
		return TDROPINFO();

	TDROPINFO vResult;

	if (m_bDropTarget)
	{
		if (m_bDropTarget)
		{
			for (BYTE j = 0; j < MAX_HOTKEY_POS / 2; j++)
			{
				if (!m_pIcon[j]->HitTest(point))
					continue;

				vResult.m_bDrop = TRUE;
				vResult.m_bSlotID = j;

				return vResult;
			}
		}
	}

	return vResult;
}