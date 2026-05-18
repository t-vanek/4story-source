#include "StdAfx.h"
#include "TFameRank.h"
#include "GuildSkillsDlg.h"
#include "TClientGame.h"
#include "Resource.h"
#include "TClient.h"
#include "TClientWnd.h"

BYTE CGuildSkillsDlg::m_bTabIndex = TGUILD_SKILL;

#define CAT_MAX				6

CGuildSkillsDlg::CGuildSkillsDlg(TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc)
	: ITInnerFrame(pParent, pDesc, TGUILD_SKILL)
{
	static constexpr DWORD TimeTextID[SKILL_CNT] = 
	{
		28228,
		28230,
		28240,
		28241,
		28242,
		28243, //ROW
		28223,
		28225,
		28244,
		28245,
		28246,
		28247, //ROW
		28248,
		28235,
		28233,
		28238,
		28249,
		28250
	};

	static constexpr DWORD TimeValueID[SKILL_CNT]
	{
		28229,
		28231,
		28251,
		28252,
		28253,
		28254, //ROW
		28224,
		28227,
		28255,
		28256,
		28257,
		28258, //ROW
		28259,
		28236,
		28234,
		28239,
		28260,
		28261
	};

	static constexpr DWORD ExpiredID[SKILL_CNT] =
	{
		28262,
		28263,
		28232,
		28264,
		28265,
		28266, //ROW
		28267,
		28268,
		28226,
		28269,
		28270,
		28271, //ROW
		28237,
		28272,
		28273,
		28274,
		28275,
		28276
	};

	static constexpr DWORD BuyID[SKILL_CNT] = 
	{
		4505,
		4506,
		4507,
		4508,
		4509,
		4510, //ROW
		4511,
		4512,
		4514,
		4515,
		4516,
		4517, //ROW
		4518,
		4519,
		28192,
		28193,
		28194,
		28195
	};

	static constexpr DWORD IconID[SKILL_CNT] = 
	{
		17070,
		17071,
		17072,
		17073,
		17074,
		17075,
		17076,
		17077,
		17078,
		17079,
		17080,
		17081,
		17270,
		17360,
		28180,
		28181,
		28182,
		28183
	};

	static constexpr DWORD LevelID[SKILL_CNT] = 
	{
		5412,
		5413,
		5414,
		5415,
		5416,
		5417, //ROW
		5418,
		5419,
		5420,
		5468,
		5469,
		5470,
		5471,
		5678,
		28196,
		28197,
		28198,
		28199
	};

	for (BYTE i = 0; i < SKILL_CNT; ++i)
	{
		m_Icon[i] = (TImageList*)FindKid(IconID[i]);
		m_Renew[i] = (TButton*)FindKid(28277 + i);
		m_Buy[i] = (TButton*)FindKid(BuyID[i]);
		m_Level[i] = FindKid(LevelID[i]);

		m_Expired[i] = FindKid(ExpiredID[i]);
		m_TimeText[i] = FindKid(TimeTextID[i]);
		m_TimeVal[i] = FindKid(TimeValueID[i]);

		m_Expired[i]->m_strText = "Expired";
		m_TimeText[i]->m_strText = "Duration";
		m_TimeVal[i]->m_strText.Empty();

		m_Level[i]->m_strText = "0/10";
	}

	for (auto Component : m_kids)
	{
		if (Component->m_pDESC.lock()->m_vCOMP.m_vImageID[0] == 926 || Component->m_pDESC.lock()->m_vCOMP.m_vImageID[0] == 11699 || Component->m_pDESC.lock()->m_vCOMP.m_vImageID[0] == 382 || (Component->m_pDESC.lock()->m_vCOMP.m_bType == 6 && !Component->m_pDESC.lock()->m_vCOMP.m_strText.IsEmpty()))
			RemoveKid(Component);
	}

	m_MyPointBar = (TGauge*)FindKid(28297);
	m_GuildPointBar = (TGauge*)FindKid(28295);

	m_MyPointBar->SetStyle(TGS_GROW_RIGHT);
	m_GuildPointBar->SetStyle(TGS_GROW_RIGHT);

	m_MyPointTxt = FindKid(28298);
	m_GuildPointTxt = FindKid(28296);

	m_GuildLevelTxt = FindKid(28204);
	m_GuildLevelVal = FindKid(28205);

	m_GuildSPointTxt = FindKid(28206);
	m_GuildSPointVal = FindKid(28207);

	m_GuildHitChanceTxt = FindKid(28209);
	m_GuildHitChanceVal = FindKid(28210);

	m_GuildEvadeTxt = FindKid(28211);
	m_GuildEvadeVal = FindKid(28212);

	m_MyLevelTxt = FindKid(28214);
	m_MyLevelVal = FindKid(28215);

	m_MySPointTxt = FindKid(28216);
	m_MySPointVal = FindKid(28217);

	m_MyHitChanceTxt = FindKid(28219);
	m_MyHitChanceVal = FindKid(28220);

	m_MyEvadeTxt = FindKid(28221);
	m_MyEvadeVal = FindKid(28222);

	for (BYTE i = 0; i < 2; ++i)
	{
		m_NotInGuildTxt[i] = FindKid(28312 + i);
		m_NotInGuildTxt[i]->MoveComponent(CPoint(275, 265));
	}

	SetGuildData(0, 0, 0, 0, 0);
	SetPlayerData(0, 0, 0, 0, 0);

	m_OwnedGuildSkill.clear();
	m_GuildSkill.clear();

	m_GMSkill = { 3000,3002,3301 };
	m_VGMSkill = { 3001, 3300, 3302 };
	m_MSkill = { 3600,3602,3603,3605 };
}

CGuildSkillsDlg::~CGuildSkillsDlg()
{
	for (auto Owned : m_OwnedGuildSkill)
		SAFE_DELETE(Owned);
	m_OwnedGuildSkill.clear();
}

void CGuildSkillsDlg::SetCurMode()
{
	CTGuildCommander* pTGUILDCMDER = CTGuildCommander::GetInstance();
	CTGuildCommander::GuildDetInfo Info = pTGUILDCMDER->GetGuildDetInfo();

	CString XD;
	//XD.Format("StatLv : %d, Point : %d, Exp: %d, MyLv : %d, MyPoint : %d", Info.m_StatLevel, Info.m_StatPoint, Info.m_StatExp, Info.m_MyStatLevel, Info.m_MyStatPoint);
	//CTClientGame::DebugMSG(XD);

	SetGuildData(Info.m_StatLevel, Info.m_StatPoint, Info.m_StatExp, CALCULATE_HITCHANCE(Info.m_StatLevel), CALCULATE_EVADE(Info.m_StatLevel));
	SetPlayerData(Info.m_MyStatLevel, Info.m_MyStatPoint, Info.m_MyStatExp, CALCULATE_ARMOR(Info.m_MyStatLevel), CALCULATE_DAMAGE(Info.m_MyStatLevel));
}
// ====================================================================

// ====================================================================
void CGuildSkillsDlg::RequestInfo()
{
	auto pSession = CTClientGame::GetInstance()->GetSession();
	if (pSession)
		pSession->SendCS_GUILDINFO_REQ();
}
// --------------------------------------------------------------------
void CGuildSkillsDlg::ResetInfo()
{
	SetCurMode();
}
// ====================================================================

// ====================================================================
void CGuildSkillsDlg::OnLButtonDown(UINT nFlags, CPoint pt)
{
	SetCurMode();

	ITInnerFrame::OnLButtonDown(nFlags, pt);
}

void CGuildSkillsDlg::OnLButtonUp(UINT nFlags, CPoint pt)
{
	BYTE Action = GS_COUNT;
	WORD Args[] = { 0 };
	BYTE Index = T_INVALID;

	for (BYTE i = 0; i < SKILL_CNT; ++i)
	{
		GuildSkill* Skill = (GuildSkill*)m_Icon[i]->GetParam(0);
		if (!Skill)
			continue;

		if (!HasPermissionForSkill(Skill->pTSKILL->m_wSkillID))
			continue;

		if (m_Buy[i]->HitTest(pt)) {
			Action = GS_BUY;
			if (Skill->pTSKILL)
				Args[0] = Skill->pTSKILL->m_wSkillID;
			break; 
		}

		if (m_Renew[i]->HitTest(pt)) {
			auto ClientGame = CTClientGame::GetInstance();
			if (ClientGame->GetMainChar()->m_dwPvPUseablePoint < SKILL_RENEW_PRICE) {
				CString strError; strError.Format("You do not have enough credits to renew this skill (%d)", SKILL_RENEW_PRICE);
				ClientGame->GetMainWnd()->MessageBoxOK(strError, TSTR_OK, GM_CLOSE_MSGBOX, GM_CLOSE_MSGBOX);
				break;
			}

			ClientGame->GetMainWnd()->MessageBoxYesNo("Do you really wnat to renew this skill for 300 Credits?", TSTR_YES, TSTR_NO, TCOMMAND(GM_GSKILL_RENEW_YES, Skill->pTSKILL->m_wSkillID), GM_CLOSE_MSGBOX);
			break;
		}
	}
	
	if (Action != GS_COUNT)
	{
		auto pSession = CTClientGame::GetInstance()->GetSession();
		if (pSession) {
			pSession->SendCS_GUILDSKILLACTION_REQ(Action, Args);
			pSession->SendCS_GUILDINFO_REQ();
		}
	}

	return CTClientUIBase::OnLButtonDown(nFlags, pt);
}

// ====================================================================
void CGuildSkillsDlg::SwitchFocus(TComponent *pCandidate)
{
	if (m_pFocus == pCandidate)
		return;

	if (m_pFocus)
		m_pFocus->SetFocus(FALSE);

	if (pCandidate)
		pCandidate->SetFocus(TRUE);

	m_pFocus = pCandidate;
}
// ====================================================================
ITDetailInfoPtr CGuildSkillsDlg::GetTInfoKey(const CPoint& point)
{
	ITDetailInfoPtr pInfo;

	for (BYTE i = 0; i < SKILL_CNT; ++i)
	{
		if (m_Icon[i]->HitTest(point))
		{
			GuildSkill* GSkill = (GuildSkill*) m_Icon[i]->GetParam(0);
			if (!GSkill)
				continue;

			if (!GSkill->pTSKILL)
				continue;

			CRect rc;
			GetComponentRect(&rc);

			pInfo = CTDetailInfoManager::NewSkillInst(
				GSkill->pTSKILL, GSkill->pSKILL ? GSkill->pSKILL->m_bLevel : 1, FALSE, rc);

			CPoint pt;
			m_Icon[i]->GetComponentPos(&pt);
			m_Icon[i]->ComponentToScreen(&pt);

			pInfo->SetDir(TRUE, TRUE, TRUE, TRUE, pt);
		}
	}

	return pInfo;
}

void CGuildSkillsDlg::ShowComponent(BOOL bVisible)
{
	ITInnerFrame::ShowComponent(bVisible);

	if (bVisible) {
		RequestInfo();
		CheckShow();
	}
}

auto CGuildSkillsDlg::ClearSkill() -> void
{
	for (auto& Skill : m_OwnedGuildSkill)
		UpdateSkill(Skill->pTSKILL->m_wSkillID, 0, 0);
}

void CGuildSkillsDlg::CheckShow()
{
	BOOL Show = FALSE;
	if (m_bVisible)
	{
		auto Char = CTClientGame::GetInstance()->GetMainChar();

		static constexpr WORD wExcept[] =
		{
			28200,
			28201,
			28202,
			28203,
			28213
		};

		Show = Char && Char->m_dwGuildID;
		for (auto Component : m_kids)
		{
			for (BYTE i = 0; i < ARRAY_LEN(wExcept); ++i)
				if (Component->m_id == wExcept[i])
					continue;

			Component->ShowComponent(Show);
		}

		for (BYTE i = 0; i < 2; ++i)
			m_NotInGuildTxt[i]->ShowComponent(!Show);
	}

	if (Show)
	{
		BYTE c_ID = 0;
		for (BYTE i = 0; i < CAT_MAX; ++i)
		{
			c_ID = i;

			m_Icon[i]->ShowComponent(c_ID < m_GMSkill.size());
			m_Level[i]->ShowComponent(c_ID < m_GMSkill.size());
			m_Buy[i]->ShowComponent(c_ID < m_GMSkill.size());
			m_Expired[i]->ShowComponent(c_ID < m_GMSkill.size());
			m_Renew[i]->ShowComponent(c_ID < m_GMSkill.size());
			m_TimeText[i]->ShowComponent(c_ID < m_GMSkill.size());
			m_TimeVal[i]->ShowComponent(c_ID < m_GMSkill.size());

			GuildSkill* Skill = GetGuildSkill(m_GMSkill[c_ID]);
			if (Skill)
				UpdateSkill(Skill->pTSKILL->m_wSkillID, Skill->m_Level, Skill->m_tEndTime);
		}

		for (BYTE i = CAT_MAX; i < CAT_MAX * 2; ++i)
		{
			c_ID = i - CAT_MAX;

			m_Icon[i]->ShowComponent(c_ID < m_VGMSkill.size());
			m_Level[i]->ShowComponent(c_ID < m_VGMSkill.size());
			m_Buy[i]->ShowComponent(c_ID < m_VGMSkill.size());
			m_Expired[i]->ShowComponent(c_ID < m_VGMSkill.size());
			m_Renew[i]->ShowComponent(c_ID < m_VGMSkill.size());
			m_TimeText[i]->ShowComponent(c_ID < m_VGMSkill.size());
			m_TimeVal[i]->ShowComponent(c_ID < m_VGMSkill.size());

			GuildSkill* Skill = GetGuildSkill(m_VGMSkill[c_ID]);
			if (Skill)
				UpdateSkill(Skill->pTSKILL->m_wSkillID, Skill->m_Level, Skill->m_tEndTime);
		}

		for (BYTE i = CAT_MAX * 2; i < CAT_MAX * 3; ++i)
		{
			c_ID = i - (CAT_MAX * 2);

			m_Icon[i]->ShowComponent(c_ID < m_MSkill.size());
			m_Level[i]->ShowComponent(c_ID < m_MSkill.size());
			m_Buy[i]->ShowComponent(c_ID < m_MSkill.size());
			m_Expired[i]->ShowComponent(c_ID < m_MSkill.size());
			m_Renew[i]->ShowComponent(c_ID < m_MSkill.size());
			m_TimeText[i]->ShowComponent(c_ID < m_MSkill.size());
			m_TimeVal[i]->ShowComponent(c_ID < m_MSkill.size());

			GuildSkill* Skill = GetGuildSkill(m_MSkill[c_ID]);
			if (Skill)
				UpdateSkill(Skill->pTSKILL->m_wSkillID, Skill->m_Level, Skill->m_tEndTime);
		}
	}
}

void CGuildSkillsDlg::SetGuildData(BYTE CombatLevel, BYTE SkillPoints, DWORD wCombatExp, BYTE HitChance, BYTE Evade)
{
	m_GuildPointBar->SetGauge(wCombatExp, CALCULATE_NEXTGEXP(CombatLevel);
	m_GuildPointTxt->m_strText.Format("%d/%d", wCombatExp, CALCULATE_NEXTGEXP(CombatLevel);
	m_GuildLevelVal->m_strText.Format("%d", CombatLevel);
	m_GuildSPointVal->m_strText.Format("%d", SkillPoints);
	m_GuildHitChanceVal->m_strText.Format("%d", HitChance);
	m_GuildEvadeVal->m_strText.Format("%d", Evade);
}

void CGuildSkillsDlg::SetPlayerData(BYTE CombatLevel, BYTE SkillPoints, DWORD wCombatExp, BYTE HitChance, BYTE Evade)
{
	m_MyPointBar->SetGauge(wCombatExp, CALCULATE_NEXTEXP(CombatLevel);
	m_MyPointTxt->m_strText.Format("%d/%d", wCombatExp, CALCULATE_NEXTEXP(CombatLevel);
	m_MyLevelVal->m_strText.Format("%d", CombatLevel);
	m_MySPointVal->m_strText.Format("%d", SkillPoints);
	m_MyHitChanceVal->m_strText.Format("%d", HitChance);
	m_MyEvadeVal->m_strText.Format("%d", Evade);
}

void CGuildSkillsDlg::InitSkill()
{
	m_GuildSkill.insert(std::make_pair(GUILD_DUTY_CHIEF, m_GMSkill));
	m_GuildSkill.insert(std::make_pair(GUILD_DUTY_VICECHIEF, m_VGMSkill));
	m_GuildSkill.insert(std::make_pair(GUILD_DUTY_CHIEF, m_MSkill));

	BYTE c_ID = 0;

	for (BYTE i = 0; i < CAT_MAX; ++i)
	{
		c_ID = i;
		if (c_ID < m_GMSkill.size())
		{
			GuildSkill* GSkill = new GuildSkill();
			GSkill->pTSKILL = CTChart::FindTSKILLTEMP(m_GMSkill[c_ID]);
			GSkill->pSKILL = nullptr;
			GSkill->m_Level = 0;
			GSkill->m_Expired = FALSE;
			GSkill->m_tEndTime = 0;

			m_Icon[i]->SetCurImage(CTChart::FindTSKILLTEMP(m_GMSkill[c_ID])->m_wIconID);
			m_Icon[i]->SaveParam(DWORD_PTR(GSkill));

			m_OwnedGuildSkill.push_back(GSkill);
		}
	}

	for (BYTE i = CAT_MAX; i < CAT_MAX * 2; ++i)
	{
		c_ID = i - CAT_MAX;
		if (c_ID < m_VGMSkill.size())
		{
			GuildSkill* GSkill = new GuildSkill();
			GSkill->pTSKILL = CTChart::FindTSKILLTEMP(m_VGMSkill[c_ID]);
			GSkill->pSKILL = nullptr;
			GSkill->m_Level = 0;
			GSkill->m_Expired = FALSE;
			GSkill->m_tEndTime = 0;

			m_Icon[i]->SetCurImage(CTChart::FindTSKILLTEMP(m_VGMSkill[c_ID])->m_wIconID);
			m_Icon[i]->SaveParam(DWORD_PTR(GSkill));

			m_OwnedGuildSkill.push_back(GSkill);
		}
	}

	for (BYTE i = CAT_MAX * 2; i < CAT_MAX * 3; ++i)
	{
		c_ID = i - (CAT_MAX * 2);
		if (c_ID < m_MSkill.size())
		{
			GuildSkill* GSkill = new GuildSkill();
			GSkill->pTSKILL = CTChart::FindTSKILLTEMP(m_MSkill[c_ID]);
			GSkill->pSKILL = nullptr;
			GSkill->m_Level = 0;
			GSkill->m_Expired = FALSE;
			GSkill->m_tEndTime = 0;

			m_Icon[i]->SetCurImage(CTChart::FindTSKILLTEMP(m_MSkill[c_ID])->m_wIconID);
			m_Icon[i]->SaveParam(DWORD_PTR(GSkill));

			m_OwnedGuildSkill.push_back(GSkill);
		}
	}
}

void CGuildSkillsDlg::UpdateSkill(WORD wSkillID, BYTE bLevel, const CTime& tEndTime)
{
	for (BYTE i = 0; i < CAT_MAX * 3; ++i)
	{
		GuildSkill* Skill = (GuildSkill*) m_Icon[i]->GetParam(0);
		if (Skill && Skill->pTSKILL && Skill->pTSKILL->m_wSkillID == wSkillID)
		{
			Skill->pSKILL = CTClientGame::GetInstance()->GetMainChar()->FindTSkill(wSkillID);
			Skill->m_Level = bLevel;
			Skill->m_tEndTime = tEndTime < CTClientApp::m_dCurDate ? 0 : tEndTime.GetTime();

			m_TimeText[i]->ShowComponent(Skill->pSKILL && Skill->m_tEndTime && Skill->m_Level);
			m_TimeVal[i]->ShowComponent(Skill->pSKILL && Skill->m_tEndTime && Skill->m_Level);
			m_Renew[i]->ShowComponent(!Skill->m_tEndTime && Skill->m_Level && HasPermissionForSkill(Skill->pTSKILL->m_wSkillID));
			m_Expired[i]->ShowComponent(!Skill->m_tEndTime && Skill->m_Level);
			m_Buy[i]->ShowComponent(Skill->m_Level < Skill->pTSKILL->m_bMaxLevel && HasPermissionForSkill(Skill->pTSKILL->m_wSkillID) && HasPointsForSkill(Skill->pTSKILL->m_wSkillID) && Skill->m_tEndTime);

			m_Level[i]->m_strText.Format("%d/%d", bLevel, Skill->pTSKILL->m_bMaxLevel);
			m_Icon[i]->EnableComponent(HasPermissionForSkill(Skill->pTSKILL->m_wSkillID));
			if (!HasPermissionForSkill(Skill->pTSKILL->m_wSkillID))
				m_Level[i]->m_strText = "?";

			CString strPERIOD;
			CTimeSpan timeSpan = tEndTime - CTClientApp::m_dCurDate;

			if (timeSpan.GetDays() > 0)
				strPERIOD = CTChart::Format(TSTR_FMT_PET_EXTPERIOD, timeSpan.GetDays());
			else
			{
				if (timeSpan.GetHours() > 0)
					strPERIOD = CTChart::Format(TSTR_FMT_PET_EXTPERIOD_HOUR_MIN, timeSpan.GetHours(), timeSpan.GetMinutes());
				else
					strPERIOD = CTChart::Format(TSTR_FMT_PET_EXTPERIOD_MIN, timeSpan.GetMinutes());
			}

			m_TimeVal[i]->m_strText = strPERIOD;
		}
	}
}

BOOL CGuildSkillsDlg::IsEnabledSkill(WORD wSkillID)
{
	for (auto& Owned : m_OwnedGuildSkill)
		if (Owned->pTSKILL->m_wSkillID == wSkillID && Owned->m_tEndTime)
			return TRUE;

	return FALSE;
}

auto CGuildSkillsDlg::HasPermissionForSkill(WORD wSkillID) -> BOOL
{
	auto Char = CTClientGame::GetInstance()->GetMainChar();
	if (!Char)
		return FALSE;

	std::vector<WORD>* Skills;

	for (BYTE i = 0; i < 3; ++i)
	{
		switch (i)
		{
		case GUILD_DUTY_NONE:
			Skills = &m_MSkill;
			break;
		case GUILD_DUTY_VICECHIEF:
			Skills = &m_VGMSkill;
			break;
		case GUILD_DUTY_CHIEF:
			Skills = &m_GMSkill;
			break;
		}

		for (auto Skill : *Skills) {
			if (Skill == wSkillID)
				return Char->m_bGuildDuty >= i;
		}
	}

	return FALSE;
}

auto CGuildSkillsDlg::HasPointsForSkill(WORD wSkillID) -> BOOL
{
	auto Char = CTClientGame::GetInstance()->GetMainChar();
	auto Info = CTGuildCommander::GetInstance()->GetGuildDetInfo();
	if (!Char)
		return FALSE;

	std::vector<WORD>* Skills;

	for (BYTE i = 0; i < 3; ++i)
	{
		switch (i)
		{
		case GUILD_DUTY_NONE:
			Skills = &m_MSkill;
			break;
		case GUILD_DUTY_VICECHIEF:
			Skills = &m_VGMSkill;
			break;
		case GUILD_DUTY_CHIEF:
			Skills = &m_GMSkill;
			break;
		}

		for (auto Skill : *Skills) {
			if (Skill == wSkillID)
				return i < GUILD_DUTY_VICECHIEF ? Info.m_MyStatPoint : Info.m_StatPoint;
		}
	}

	return FALSE;
}

GuildSkill* CGuildSkillsDlg::GetGuildSkill(WORD wSkillID)
{
	for (auto& Owned : m_OwnedGuildSkill)
		if (Owned->pTSKILL->m_wSkillID == wSkillID)
			return Owned;

	return nullptr;
}

BYTE CGuildSkillsDlg::OnBeginDrag(LPTDRAG pDRAG, CPoint point)
{
	pDRAG = &CTClientGame::GetInstance()->m_vDRAG;
	pDRAG->m_bFrameID = TFRAME_GUILD_BASE_NEW;

	for (BYTE i = 0; i < SKILL_CNT; ++i)
	{
		if (m_Icon[i]->HitTest(point) && m_Icon[i]->GetCurImage())
		{
			if (pDRAG)
			{
				GuildSkill* GSkill = (GuildSkill*)m_Icon[i]->GetParam(0);
				if (!GSkill) { continue; }

				if (!HasPermissionForSkill(GSkill->pTSKILL->m_wSkillID))
					continue;

				pDRAG->m_bSlotID = i;
				CPoint pt;
				pDRAG->m_pIMAGE = new TImageList(
					NULL,
					*m_Icon[i]);

				pDRAG->m_pIMAGE->SetCurImage(m_Icon[i]->GetCurImage());
				pDRAG->m_pIMAGE->m_strText.Empty();

				pDRAG->m_dwParam = GSkill->pTSKILL->m_wSkillID;

				m_Icon[i]->GetComponentPos(&pt);
				m_Icon[i]->ComponentToScreen(&pt);
				m_Icon[i]->m_strText.Empty();

				pDRAG->m_pIMAGE->ShowComponent(TRUE);
				pDRAG->m_pIMAGE->MoveComponent(pt);

				return TRUE;
			}
		}
	}

	return FALSE;
} 