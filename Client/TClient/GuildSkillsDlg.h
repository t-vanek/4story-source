#pragma once



#define SKILL_CNT	18

struct GuildSkill
{
public:
	LPTSKILL pTSKILL;
	CTClientSkill* pSKILL;

	BYTE m_Level;
	__time64_t m_tEndTime;
	BOOL m_Expired;

public:
	BOOL DoesOwn() { return pSKILL != nullptr; };
	void SetSkill(CTClientSkill* Skill)
	{
		pSKILL = Skill;
		pTSKILL = Skill->m_pTSKILL;
	};
};

class CGuildSkillsDlg : public ITInnerFrame
{
public:
	static BYTE		m_bTabIndex;

	TImageList* m_Icon[SKILL_CNT];
	TComponent* m_Level[SKILL_CNT];
	TButton* m_Renew[SKILL_CNT];
	TComponent* m_Expired[SKILL_CNT];
	TComponent* m_TimeText[SKILL_CNT];
	TComponent* m_TimeVal[SKILL_CNT];
	TButton* m_Buy[SKILL_CNT];
	
	TComponent* m_GuildLevelTxt;
	TComponent* m_GuildLevelVal;

	TComponent* m_GuildSPointTxt;
	TComponent* m_GuildSPointVal;

	TComponent* m_GuildHitChanceTxt;
	TComponent* m_GuildHitChanceVal;

	TComponent* m_GuildEvadeTxt;
	TComponent* m_GuildEvadeVal;

	TComponent* m_MyPointTxt;
	TComponent* m_GuildPointTxt;

	TComponent* m_MyLevelTxt;
	TComponent* m_MyLevelVal;

	TComponent* m_MySPointTxt;
	TComponent* m_MySPointVal;

	TComponent* m_MyHitChanceTxt;
	TComponent* m_MyHitChanceVal;

	TComponent* m_MyEvadeTxt;
	TComponent* m_MyEvadeVal;

	TGauge* m_MyPointBar;
	TGauge* m_GuildPointBar;

	TComponent* m_NotInGuildTxt[2];
protected:
	std::map<BYTE, std::vector<WORD>> m_GuildSkill;
	std::vector<GuildSkill*> m_OwnedGuildSkill;
public:
	void SetCurMode();
	void InitSkill();

	void UpdateSkill(WORD wSkillID, BYTE bLevel, const CTime& tEndTime);

	BOOL IsEnabledSkill(WORD wSkillID);
	auto HasPermissionForSkill(WORD wSkillID)->BOOL;
	auto HasPointsForSkill(WORD wSkillID)->BOOL;
	auto ClearSkill() -> void;
	GuildSkill * GetGuildSkill(WORD wSkillID);
private:
	std::vector<WORD> m_GMSkill;
	std::vector<WORD> m_VGMSkill;
	std::vector<WORD> m_MSkill;
public:
	virtual void RequestInfo();
	virtual void ResetInfo();

	void CheckShow();

	void SetGuildData(BYTE CombatLevel, BYTE SkillPoints, DWORD wCombatExp, BYTE HitChance, BYTE Evade);
	void SetPlayerData(BYTE CombatLevel, BYTE SkillPoints, DWORD wCombatExp, BYTE HitChance, BYTE Evade);
public:
	virtual void OnLButtonDown(UINT nFlags, CPoint pt);
	virtual void OnLButtonUp(UINT nFlags, CPoint pt);
	virtual void SwitchFocus(TComponent *pCandidate);
	virtual ITDetailInfoPtr GetTInfoKey(const CPoint& point);
	virtual void ShowComponent(BOOL bVisible = 1);
	BYTE OnBeginDrag(LPTDRAG pDRAG, CPoint point);
public:
	CGuildSkillsDlg(TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc);
	virtual ~CGuildSkillsDlg();
};