#pragma once

struct _HotkeyUsed
{
	LPTSKILL pSKILL;
	DWORD m_dwUsedTick;
	BYTE m_bUsed; //Reused Only
};

struct _HotkeyReused
{
	BYTE m_bUsed;
};

class MacroProtection
{
private:
	CTClientGame* m_pGAME;
public:
	std::vector<_HotkeyUsed> m_vUsedHotkey;
	std::map<WORD, _HotkeyUsed> m_mapReusedHotkey;
public:
	void Update();
	void NotifySkillUse(LPTSKILL pSKILL);
	std::vector<LPTSKILL> GetAllSkillInSameTree(WORD wSkillID);
	void NotifySkillWentThrough(LPTSKILL pSKILL);
	void SetCooldown(WORD wSkillID);
public:
	MacroProtection(CTClientGame* pTGAME);
	virtual ~MacroProtection();
};