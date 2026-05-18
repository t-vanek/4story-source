#include "StdAfx.h"
#include "MacroProtection.h"
#include "TClientGame.h"

#define MAX_SKILL_PER_SEC			20
#define MAX_SKILL_2HTK_PER_SEC		2
#define SKILL_MACRO_CD				0

MacroProtection::MacroProtection(CTClientGame* pTGAME)
{
	m_pGAME = pTGAME;
	m_vUsedHotkey.clear();
	m_mapReusedHotkey.clear();
}

MacroProtection::~MacroProtection()
{

}

void MacroProtection::Update()
{
	std::vector<_HotkeyUsed>::iterator& itHotkeyUsed = m_vUsedHotkey.begin();
	while (itHotkeyUsed != m_vUsedHotkey.end())
	{
		if (GetTickCount() >= itHotkeyUsed->m_dwUsedTick + 30000) {
			itHotkeyUsed = m_vUsedHotkey.erase(itHotkeyUsed);
			continue;
		}

		itHotkeyUsed++;
	}

	for (auto ReusedHotkey : m_mapReusedHotkey)
	{
		if (GetTickCount() >= ReusedHotkey.second.m_dwUsedTick + 500) {
			if (m_mapReusedHotkey.find(ReusedHotkey.second.pSKILL->m_wSkillID) != m_mapReusedHotkey.end())
				m_mapReusedHotkey.erase(ReusedHotkey.second.pSKILL->m_wSkillID);
		}
	}
}

void MacroProtection::NotifySkillUse(LPTSKILL pSKILL)
{
	if (!pSKILL || pSKILL->m_bContinue || pSKILL->m_bLoop || (pSKILL->m_dwSpellTick || !CTChart::GetTACTION(pSKILL->m_bActionID[TSKILLACTION_MOVEACTIVATE])->m_bSubAction))
		return;

	Update();

	_HotkeyUsed UsedHotkey
	{
		pSKILL,
		GetTickCount(),
		0
	};

	m_vUsedHotkey.push_back(UsedHotkey);

	std::map<WORD, _HotkeyUsed>::iterator finderReused = m_mapReusedHotkey.find(pSKILL->m_wSkillID);
	if (finderReused != m_mapReusedHotkey.end()) {
		finderReused->second.m_bUsed += 1;
	}
	else {
		m_mapReusedHotkey.insert(std::map<WORD, _HotkeyUsed>::value_type(pSKILL->m_wSkillID, UsedHotkey));
	}

	Update();
}

std::vector<LPTSKILL> MacroProtection::GetAllSkillInSameTree(WORD wSkillID)
{
	CTClientChar* pMainChar = m_pGAME->GetMainChar();
	std::vector<LPTSKILL> vSkill;

	for (BYTE i = 0; i < TSKILLTAB_COUNT; i++)
	{
		for (BYTE j = 0; j < TSKILLSLOT_COUNT; j++)
		{
			if (wSkillID == CTChart::GetTSKILLID(pMainChar->m_bContryID, pMainChar->m_bClassID, i, j))
			{
				for (BYTE l = 0; l < TSKILLSLOT_COUNT; ++l)
				{
					LPTSKILL pTSKILL = CTChart::FindTSKILLTEMP(CTChart::GetTSKILLID(pMainChar->m_bContryID, pMainChar->m_bClassID, i, l));
					if (pTSKILL && !(pTSKILL->m_dwSpellTick || !CTChart::GetTACTION(pTSKILL->m_bActionID[TSKILLACTION_MOVEACTIVATE])->m_bSubAction)) {
						vSkill.push_back(pTSKILL);
					}
				}

				return vSkill;
			}
		}
	}

	return std::vector<LPTSKILL>();
}

void MacroProtection::NotifySkillWentThrough(LPTSKILL pSKILL)
{
	CString debug;

	if (!pSKILL || pSKILL->m_bContinue || pSKILL->m_bLoop)
		return;

	Update();

	if (m_vUsedHotkey.size() >= MAX_SKILL_PER_SEC)
	{
		SetCooldown(pSKILL->m_wSkillID);
	}

	BYTE AbusedHotkeyCnt = 0;
	BYTE MaxUsed = 0;
	WORD wSkillID = 0;

	for (auto& ReusedHotkey : m_mapReusedHotkey)
	{
		if (ReusedHotkey.second.m_bUsed >= MAX_SKILL_2HTK_PER_SEC) 
		{
			if (ReusedHotkey.second.m_bUsed >= MaxUsed)
			{
				MaxUsed = ReusedHotkey.second.m_bUsed;
				wSkillID = ReusedHotkey.second.pSKILL->m_wSkillID;
			}

			AbusedHotkeyCnt++;
		}
	}

	if (AbusedHotkeyCnt >= 2)
	{
	//	m_pGAME->DebugMSG("more than 2 per sec");
		SetCooldown(wSkillID);
	}
}

void MacroProtection::SetCooldown(WORD wSkillID)
{
	CString debug;

	CTClientChar* pMainChar = m_pGAME->GetMainChar();
	for (auto& Skill : GetAllSkillInSameTree(wSkillID))
	{
		CTClientSkill* pSkill = pMainChar->FindTSkill(Skill->m_wSkillID);
		if (pSkill)
		{
			if (!pSkill->m_dwTick && !pSkill->m_dwExceptTick)
				pSkill->m_dwExceptTick = SKILL_MACRO_CD;

			m_pGAME->ResetSkillICON();

			//debug.Format("Setting Tick to Skill : %s", pSkill->m_pTSKILL->m_strNAME);
			//m_pGAME->DebugMSG(debug);
		}
	}
}