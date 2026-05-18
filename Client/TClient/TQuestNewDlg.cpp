#include "stdafx.h"
#include "Resource.h"
#include "TClientGame.h"
#include "TClientWnd.h"
#include "TQuestNewDlg.h"


CPoint CQuestNewDlg::m_ptPOS = CPoint(0, 0);
DWORD CQuestNewDlg::m_DefaultQColor = 0;

#define GIVE_UP		CString("Give Up")
#define ACCEPT		CString("Accept")
#define NEXT		CString("Next")
#define RUN			CString("Run")
#define COMPLETE	CString("Complete")

struct binary_qclass
{
	bool operator () (const CTClientQClass *_Left, const CTClientQClass *_Right) const
	{
		return _Left->m_pTQCLASS && _Left->m_pTQCLASS->m_bMAIN ? true : false;
	};
};

struct binary_tquest
{
	bool operator () (const CTClientQuest *_Left, const CTClientQuest *_Right) const
	{
		return binary_quest()(_Left->m_pTQUEST, _Right->m_pTQUEST);
	};
};

CQuestNewDlg::CQuestNewDlg(TComponent* pParent, FRAMEDESC_SHAREDPTR pDesc, CTClientChar* pHost)
	: CTClientUIBase(pParent, pDesc), m_pHost(pHost)
{
	static DWORD dwItem[TREWARDITEMCOUNT] =
	{
		ID_CTRLINST_ITEM_1,
		ID_CTRLINST_ITEM_2
	};

	static DWORD dwSkill[TREWARDITEMCOUNT] =
	{
		ID_CTRLINST_SKILL_1,
		ID_CTRLINST_SKILL_2
	};

	static DWORD dwSel[TREWARDITEMCOUNT] =
	{
		ID_CTRLINST_SEL_1,
		ID_CTRLINST_SEL_2
	};

	m_pAccept = (TButton*)FindKid(25203);
	m_pRefuse = (TButton*)FindKid(25204);

	const char* Name[3] = { "Normal Quest", "Repeatable Quest", "Event Quest" };

	for (BYTE i = 0; i < 3; ++i)
	{
		m_QuestList[i].m_pFrame = this;
		m_QuestList[i].LoadComponent();
		m_QuestList[i].m_pQuestCat->m_strText = Name[i];
	}

	m_RightSide.m_pFrame = this;
	m_RightSide.LoadComponent();

	m_pCheckBox = (TButton*)FindKid(26132);
	m_pQuestBtn = (TButton*)FindKid(11106);
	m_pQuestName = FindKid(26564);
	m_pProgress = (TImageList*)FindKid(26134);
	m_pQuestCatReg = FindKid(26131);
	m_pQuestCat = (TButton*)FindKid(26130);

	m_pCheckBox->SaveFramedata();
	m_pQuestBtn->SaveFramedata();
	m_pQuestName->SaveFramedata();
	m_pQuestCatReg->SaveFramedata();
	m_pProgress->SaveFramedata();
	m_pQuestCat->SaveFramedata();

	FindKid(9681)->SaveFramedata();
	static_cast<TImageList*>(FindKid(8612))->SaveFramedata();
	static_cast<TImageList*>(FindKid(26139))->SaveFramedata();
	static_cast<TImageList*>(FindKid(26141))->SaveFramedata();
	static_cast<TImageList*>(FindKid(26205))->SaveFramedata();
	FindKid(26145)->SaveFramedata();

	m_pLScroll = (TScroll*)FindKid(8512);
	m_pRScroll = (TScroll*)FindKid(26136);

	m_pQuestText = FindKid(26263);

	m_pREFUSE = (TButton *)FindKid(25204);
	m_pACCEPT = (TButton *)FindKid(25203);

	m_pShowMap = FindKid(26162);

	m_pMapBorder = FindKid(26719);
	m_pMapBorder->MoveComponent(CPoint(355, 61));

	m_pMyCharPos[0] = (TImageList*)FindKid(26164);
	m_pMyCharPos[0]->SetCurImage(0);

	m_pMyCharPos[1] = (TImageList*)FindKid(26163);
	m_pMyCharPos[1]->SetCurImage(2);

	m_pMapList = (TImageList*)FindKid(26160);
	m_pMapList->MoveComponent(CPoint(355 + 24, 55 + 24));
	m_pNoTerm = (TImageList*)FindKid(7193);
	m_pNoTerm->MoveComponent(CPoint(355 + 24, 55 + 24));

	m_DefaultQColor = m_pQuestName->m_pFont->m_dwColor;

	m_Quest = nullptr;

	m_nDesiredLeftScrollY = 0;
	m_nCurLeftScrollY = 0;

	m_nCurRightScrollY = 0;
	m_nDesiredRightScrollY = 0;

	m_nMainUnitX = 0;
	m_nMainUnitZ = 0;
	m_ptMapIcon = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	m_vTCENTER = D3DXVECTOR2(0.0f, 0.0f);
	m_fTSCALE = 1.0f;
	m_pTMAP = NULL;
	m_bHideMap = TRUE;

	m_pTTOPREWARD = NULL;
	m_pTSELREWARD = NULL;
	m_pTQUEST = NULL;
	m_pHost = pHost;
	m_bPrintMSG = TRUE;
	m_pQuestDlg = this;
	m_strNPCTalk = "";
	m_strNPCTitle = "";
	m_strAnswerWhenNPCTalk = "";

	m_dwCompleteID = TSTR_GIVEUP;
	m_dwAcceptID = TSTR_GIVEUP;

	m_pACCEPT->m_menu[TNM_LCLICK] = 0;
	m_pREFUSE->m_menu[TNM_LCLICK] = 0;
}

CQuestNewDlg::~CQuestNewDlg(void)
{
}

HRESULT CQuestNewDlg::Render(DWORD dwTickCount)
{
	static CPoint pt[6] = { 0 };
	static LPTQUEST pQUEST = nullptr;

	if (IsVisible())
	{
		if (!pt[0].x)
		{
			m_pQuestCat->GetComponentPos(&pt[0]);
			m_pQuestCatReg->GetComponentPos(&pt[1]);
			m_pCheckBox->GetComponentPos(&pt[2]);
			m_pQuestBtn->GetComponentPos(&pt[3]);
			m_pQuestName->GetComponentPos(&pt[4]);
			m_pProgress->GetComponentPos(&pt[5]);
		}

		for (BYTE i = 0; i < 3; ++i) //Resynchonize
		{
			if (m_QuestList[i].GetSize())
			{
				m_QuestList[i].m_pQuestCat->MoveComponent(pt[0]);
				for (BYTE c = 0; c < m_QuestList[i].GetSize(); ++c)
				{
					if (m_QuestList[i].m_pQuestCatReg[c])
						m_QuestList[i].m_pQuestCatReg[c]->MoveComponent(CPoint(pt[1].x, pt[0].y));

					for (auto itQ : m_QuestList[i].m_mapQuest)
					{
						for (auto q : itQ.second)
						{
							if (q.m_pCheckBox)
								q.m_pCheckBox->MoveComponent(CPoint(pt[2].x, pt[0].y));
							if (q.m_pQuestBtn)
								q.m_pQuestBtn->MoveComponent(CPoint(pt[3].x, pt[0].y));
							if (q.m_pQuestName)
								q.m_pQuestName->MoveComponent(CPoint(pt[4].x, pt[0].y));
							if (q.m_pProgress)
								q.m_pProgress->MoveComponent(CPoint(pt[5].x, pt[0].y));
						}
					}
				}
			}
		}

		FLOAT fIndex = 0.0f;
		std::map<DWORD, std::vector<CQuest>>::iterator itQClassBuf;
		for (BYTE i = 0; i < 3; ++i)
		{
			if (m_QuestList[i].m_pQuestCat && m_QuestList[i].GetSize())
			{
				m_QuestList[i].m_pQuestCat->MoveComponentBy(0, INT(16 * fIndex));
				m_QuestList[i].m_pQuestCat->MoveComponentBy(0, 3);

				fIndex += 2;
			}
			else
				continue;

			if (m_QuestList[i].m_Opened)
			{
				for (std::map<DWORD, std::vector<CQuest>>::iterator& itQClass = m_QuestList[i].m_mapQuest.begin(); itQClass != m_QuestList[i].m_mapQuest.end(); ++itQClass)
				{
					for (BYTE l = 0; l < m_QuestList[i].GetSize(); ++l)
					{
						if (!m_QuestList[i].m_pQuestCatReg[l])
							continue;

						if (m_QuestList[i].m_pQuestCatReg[l]->m_dwMediaID == itQClass->first)
						{
							m_QuestList[i].m_pQuestCatReg[l]->MoveComponentBy(0, INT(16 * fIndex));
							fIndex += 1.35f;
							break;
						}
					}

					for (std::vector<CQuest>::iterator& Quest = itQClass->second.begin(); Quest != itQClass->second.end(); ++Quest)
					{
						if (Quest->m_pCheckBox)
							Quest->m_pCheckBox->MoveComponentBy(0, INT(16 * fIndex));
						if (Quest->m_pQuestBtn)
							Quest->m_pQuestBtn->MoveComponentBy(0, INT(16 * fIndex));
						if (Quest->m_pQuestName)
							Quest->m_pQuestName->MoveComponentBy(0, INT(16 * fIndex));
						if (Quest->m_pProgress)
							Quest->m_pProgress->MoveComponentBy(0, INT(16 * fIndex));

						LPTQUEST pLPQUEST = nullptr;
						if (Quest->m_Accepted)
						{
							CTClientQuest* Que = (CTClientQuest*)Quest->m_dwQuest;
							if (Que && Que->m_pTQUEST)
								pLPQUEST = Que->m_pTQUEST;
						}
						else
							pLPQUEST = (LPTQUEST)Quest->m_dwQuest;

						if (pLPQUEST && pLPQUEST->m_bType == QT_COMPLETE)
							pLPQUEST = CTChart::FindTMISSION(pLPQUEST);

						if (pLPQUEST)
							Quest->m_pQuestBtn->Select(pLPQUEST == m_pTQUEST);
						else
							continue;

						if (Quest->m_Accepted || Quest->m_Completed)
						{
							CTClientQuest* pTQUEST = nullptr;
							pTQUEST = (CTClientQuest*)Quest->m_dwQuest;

							if (Quest->m_pCheckBox)
								Quest->m_pCheckBox->Select(Quest->m_bSelected);

							if (!FindKid(Quest->m_pCheckBox->m_id))
								AddKid(Quest->m_pCheckBox);
							if (!FindKid(Quest->m_pProgress->m_id))
								AddKid(Quest->m_pProgress);

							if (pTQUEST && pTQUEST->m_pTQUEST)
							{
								if (Quest->m_Accepted)
								{
									switch (pTQUEST->GetResult())
									{
									case TTERMRESULT_COMPLETE: Quest->m_pProgress->SetCurImage(1); break;
									case TTERMRESULT_INCOMPLETE: Quest->m_pProgress->SetCurImage(2); break;
									case TTERMRESULT_FAILED: Quest->m_pProgress->SetCurImage(3); break;
									case TTERMRESULT_TIMEOUT: Quest->m_pProgress->SetCurImage(4); break;
									}
								}

								if (pTQUEST->m_bTimmer)
									Quest->m_pProgress->SetCurImage(0);
							}
						}
						fIndex += 1.25f;
					}
				}
			}
		}

		for (BYTE i = 0; i < 3; ++i)
			m_QuestList[i].MoveCatBy(m_nCurLeftScrollY);

		fIndex = (fIndex * 16.0f) - 374.0f;
		m_pLScroll->SetScrollPos((INT)fIndex, -m_nCurLeftScrollY >(WORD) fIndex ? (WORD)fIndex : -m_nCurLeftScrollY);

		FLOAT fRIndex = 0.0f;
		fRIndex = m_RightSide.m_Index - m_nCurRightScrollY - 316.0f;
		fRIndex += 24.0f;

		m_pRScroll->SetScrollPos((INT)fRIndex, -m_nCurRightScrollY > (WORD)fRIndex ? (WORD)fRIndex : -m_nCurRightScrollY);

		if (m_nCurLeftScrollY > -fIndex && m_nDesiredLeftScrollY == -1)
		{
			m_nDesiredLeftScrollY = (INT)-fIndex;
			m_nCurLeftScrollY = (INT)-fIndex;
		}

		if (m_nCurLeftScrollY < -fIndex && m_nDesiredLeftScrollY == -1)
		{
			if (fIndex < 1.0f)
			{
				m_nDesiredLeftScrollY = 0;
				m_nCurLeftScrollY = 0;
			}
			else
			{
				m_nDesiredLeftScrollY = (INT)-fIndex;
				m_nCurLeftScrollY = (INT)-fIndex;
			}
		}

		if (m_nCurLeftScrollY != m_nDesiredLeftScrollY && fIndex > 1.0f)
		{
			if (m_nCurLeftScrollY > 0)
				m_nDesiredLeftScrollY = 0;

			if (m_nCurLeftScrollY < -INT(fIndex))
				m_nDesiredLeftScrollY = -INT(fIndex);

			FLOAT fSpeed = 0.0f;

			if (m_nDesiredLeftScrollY < m_nCurLeftScrollY)
			{
				fSpeed = m_nCurLeftScrollY > 0 ? 1.0f : (FLOAT)round(log10(m_nCurLeftScrollY - m_nDesiredLeftScrollY) * 7.0f);
				m_nCurLeftScrollY -= (INT)fSpeed;

				if (m_nCurLeftScrollY < m_nDesiredLeftScrollY)
					m_nDesiredLeftScrollY = m_nCurLeftScrollY;
			}
			else
			{
				fSpeed = m_nCurLeftScrollY < -INT(fIndex) ? 1.0f : (FLOAT)round(log10(m_nDesiredLeftScrollY - m_nCurLeftScrollY) * 7.0f);
				m_nCurLeftScrollY += (INT)fSpeed;

				if (m_nCurLeftScrollY > m_nDesiredLeftScrollY)
					m_nDesiredLeftScrollY = m_nCurLeftScrollY;
			}
		}

		if (m_nCurRightScrollY != m_nDesiredRightScrollY)
		{
			if (m_nCurRightScrollY > 0)
				m_nDesiredRightScrollY = 0;

			if (m_nCurRightScrollY < -INT(fRIndex))
				m_nDesiredRightScrollY = -INT(fRIndex);

			FLOAT fSpeed = 0.0f;

			if (m_nDesiredRightScrollY < m_nCurRightScrollY)
			{
				fSpeed = m_nCurRightScrollY > 0 ? 1.0f : (FLOAT)round(log10(m_nCurRightScrollY - m_nDesiredRightScrollY) * 6.0f);
				m_nCurRightScrollY -= (INT)fSpeed;

				if (m_nCurRightScrollY < m_nDesiredRightScrollY)
					m_nDesiredRightScrollY = m_nCurRightScrollY;
			}
			else
			{
				fSpeed = m_nCurRightScrollY < -INT(fRIndex) ? 1.0f : (FLOAT)round(log10(m_nDesiredRightScrollY - m_nCurRightScrollY) * 6.0f);
				m_nCurRightScrollY += (INT)fSpeed;

				if (m_nCurRightScrollY > m_nDesiredRightScrollY)
					m_nDesiredRightScrollY = m_nCurRightScrollY;
			}

			m_RightSide.ReAlign(m_nCurRightScrollY);
		}


		CPoint pt[6];
		std::vector<DWORD_PTR> vHide; vHide.clear();

		for (BYTE i = 0; i < 3; ++i)
		{
			for (BYTE k = 0; k < m_QuestList[i].GetSize(); ++k)
			{
				m_QuestList[i].m_pQuestCatReg[k]->GetComponentPos(&pt[5]);
				if (pt[5].y > 420 || pt[5].y < 50 || !m_QuestList[i].m_Opened)
					vHide.push_back((DWORD_PTR)m_QuestList[i].m_pQuestCatReg[k]);
				else
					if (m_QuestList[i].m_pQuestCat)
						m_QuestList[i].m_pQuestCatReg[k]->ShowComponent(TRUE);
			}

			m_QuestList[i].m_pQuestCat->GetComponentPos(&pt[4]);
			if (pt[4].y > 415 || pt[4].y < 45 || !m_QuestList[i].GetSize())
				vHide.push_back((DWORD_PTR)m_QuestList[i].m_pQuestCat);
			else
				if (m_QuestList[i].m_pQuestCat)
					m_QuestList[i].m_pQuestCat->ShowComponent(TRUE);

			for (auto QClass : m_QuestList[i].m_mapQuest)
			{
				for (auto Quest : QClass.second)
				{
					if (Quest.m_pQuestBtn)
						Quest.m_pQuestBtn->GetComponentPos(&pt[0]);
					if (Quest.m_pCheckBox)
						Quest.m_pCheckBox->GetComponentPos(&pt[1]);
					if (Quest.m_pQuestName)
						Quest.m_pQuestName->GetComponentPos(&pt[2]);
					if (Quest.m_pProgress)
						Quest.m_pProgress->GetComponentPos(&pt[3]);

					if (pt[0].y > 420 || pt[3].y < 50 || !m_QuestList[i].m_Opened)
						vHide.push_back((DWORD_PTR)Quest.m_pQuestBtn);
					else
						if (Quest.m_pQuestBtn)
							Quest.m_pQuestBtn->ShowComponent(TRUE);

					if (pt[1].y > 420 || pt[3].y < 50 || !m_QuestList[i].m_Opened)
						vHide.push_back((DWORD_PTR)Quest.m_pCheckBox);
					else
						if (Quest.m_pCheckBox)
							Quest.m_pCheckBox->ShowComponent(TRUE);

					if (pt[2].y > 420 || pt[3].y < 50 || !m_QuestList[i].m_Opened)
						vHide.push_back((DWORD_PTR)Quest.m_pQuestName);
					else
						if (Quest.m_pQuestName)
							Quest.m_pQuestName->ShowComponent(TRUE);

					if (pt[3].y > 420 || pt[3].y < 50 || !m_QuestList[i].m_Opened)
						vHide.push_back((DWORD_PTR)Quest.m_pProgress);
					else
						if (Quest.m_pProgress)
							Quest.m_pProgress->ShowComponent(TRUE);

					if (m_RightSide.m_pRewardDot->m_rc.top > 410 || m_RightSide.m_pRewardDot->m_rc.top < 100 || !m_pTQUEST || !m_RightSide.m_Reward)
						m_RightSide.m_pRewardDot->ShowComponent(FALSE);
					else
						m_RightSide.m_pRewardDot->ShowComponent(TRUE);

					if (m_RightSide.m_pRewardText->m_rc.top > 410 || m_RightSide.m_pRewardText->m_rc.top < 100 || !m_pTQUEST || !m_RightSide.m_Reward)
						m_RightSide.m_pRewardText->ShowComponent(FALSE);
					else
						m_RightSide.m_pRewardText->ShowComponent(TRUE);

					if (m_RightSide.m_pTermDot->m_rc.top > 410 || m_RightSide.m_pTermDot->m_rc.top < 100 || !m_pTQUEST || !m_RightSide.m_Goal)
						m_RightSide.m_pTermDot->ShowComponent(FALSE);
					else
						m_RightSide.m_pTermDot->ShowComponent(TRUE);

					if (m_RightSide.m_pTermText->m_rc.top > 410 || m_RightSide.m_pTermText->m_rc.top < 100 || !m_pTQUEST || !m_RightSide.m_Goal)
						m_RightSide.m_pTermText->ShowComponent(FALSE);
					else
						m_RightSide.m_pTermText->ShowComponent(TRUE);

					if (m_RightSide.m_pSummaryDot->m_rc.top > 410 || m_RightSide.m_pSummaryDot->m_rc.top < 100 || !m_pTQUEST || !m_RightSide.m_Summary)
						m_RightSide.m_pSummaryDot->ShowComponent(FALSE);
					else
						m_RightSide.m_pSummaryDot->ShowComponent(TRUE);

					if (m_RightSide.m_pSummaryText->m_rc.top > 410 || m_RightSide.m_pSummaryText->m_rc.top < 100 || !m_pTQUEST || !m_RightSide.m_Summary)
						m_RightSide.m_pSummaryText->ShowComponent(FALSE);
					else
						m_RightSide.m_pSummaryText->ShowComponent(TRUE);

					if (m_RightSide.m_pDialogueDot->m_rc.top > 410 || m_RightSide.m_pDialogueDot->m_rc.top < 100 || !m_pTQUEST || !m_RightSide.m_Dialogue)
						m_RightSide.m_pDialogueDot->ShowComponent(FALSE);
					else
						m_RightSide.m_pDialogueDot->ShowComponent(TRUE);

					if (m_RightSide.m_pDialogueText->m_rc.top > 410 || m_RightSide.m_pDialogueText->m_rc.top < 100 || !m_pTQUEST || !m_RightSide.m_Dialogue)
						m_RightSide.m_pDialogueText->ShowComponent(FALSE);
					else
						m_RightSide.m_pDialogueText->ShowComponent(TRUE);

					for (BYTE i = 0; i < STATIC_MAX; ++i)
					{
						if (!FindKid(m_RightSide.m_pItemFrame[i]->m_id) || m_RightSide.m_pItemFrame[i]->m_rc.top > 410 || m_RightSide.m_pItemFrame[i]->m_rc.top < 100 || !m_pTQUEST)
							m_RightSide.m_pItemFrame[i]->ShowComponent(FALSE);
						else
							m_RightSide.m_pItemFrame[i]->ShowComponent(TRUE);

						if (!FindKid(m_RightSide.m_pSkillIcon[i]->m_id) || m_RightSide.m_pSkillIcon[i]->m_rc.top > 410 || m_RightSide.m_pSkillIcon[i]->m_rc.top < 100 || !m_pTQUEST)
							m_RightSide.m_pSkillIcon[i]->ShowComponent(FALSE);
						else
							m_RightSide.m_pSkillIcon[i]->ShowComponent(TRUE);

						if (!FindKid(m_RightSide.m_pItemIcon[i]->m_id) || m_RightSide.m_pItemIcon[i]->m_rc.top > 410 || m_RightSide.m_pItemIcon[i]->m_rc.top < 100 || !m_pTQUEST)
							m_RightSide.m_pItemIcon[i]->ShowComponent(FALSE);
						else
							m_RightSide.m_pItemIcon[i]->ShowComponent(TRUE);

						if (!FindKid(m_RightSide.m_pTitleIcon[i]->m_id) || m_RightSide.m_pTitleIcon[i]->m_rc.top > 410 || m_RightSide.m_pTitleIcon[i]->m_rc.top < 100 || !m_pTQUEST)
							m_RightSide.m_pTitleIcon[i]->ShowComponent(FALSE);
						else
							m_RightSide.m_pTitleIcon[i]->ShowComponent(TRUE);

						if (!FindKid(m_RightSide.m_pRewardName[i]->m_id) || m_RightSide.m_pRewardName[i]->m_rc.top > 410 || m_RightSide.m_pRewardName[i]->m_rc.top < 100 || !m_pTQUEST)
							m_RightSide.m_pRewardName[i]->ShowComponent(FALSE);
						else
							m_RightSide.m_pRewardName[i]->ShowComponent(TRUE);

						if (!FindKid(m_RightSide.m_pRewardName[i]->m_id) || m_RightSide.m_pRewardName[i]->m_rc.top > 410 || m_RightSide.m_pRewardName[i]->m_rc.top < 100 || !m_pTQUEST)
							m_RightSide.m_pRewardName[i]->ShowComponent(FALSE);
						else
							m_RightSide.m_pRewardName[i]->ShowComponent(TRUE);

						if (!FindKid(m_RightSide.m_pTermStr[i]->m_id) || m_RightSide.m_pTermStr[i]->m_rc.top > 410 || m_RightSide.m_pTermStr[i]->m_rc.top < 100 || !m_pTQUEST)
							m_RightSide.m_pTermStr[i]->ShowComponent(FALSE);
						else
							m_RightSide.m_pTermStr[i]->ShowComponent(TRUE);

						if (!FindKid(m_RightSide.m_pTermRes[i]->m_id) || m_RightSide.m_pTermRes[i]->m_rc.top > 410 || m_RightSide.m_pTermRes[i]->m_rc.top < 100 || !m_pTQUEST)
							m_RightSide.m_pTermRes[i]->ShowComponent(FALSE);
						else
							m_RightSide.m_pTermRes[i]->ShowComponent(TRUE);

						if (!FindKid(m_RightSide.m_pSummaryStr[i]->m_id) || m_RightSide.m_pSummaryStr[i]->m_rc.top > 410 || m_RightSide.m_pSummaryStr[i]->m_rc.top < 100 || !m_pTQUEST)
							m_RightSide.m_pSummaryStr[i]->ShowComponent(FALSE);
						else
							m_RightSide.m_pSummaryStr[i]->ShowComponent(TRUE);

						if (!FindKid(m_RightSide.m_pDialogueStr[i]->m_id) || m_RightSide.m_pDialogueStr[i]->m_rc.top > 410 || m_RightSide.m_pDialogueStr[i]->m_rc.top < 100 || !m_pTQUEST)
							m_RightSide.m_pDialogueStr[i]->ShowComponent(FALSE);
						else
							m_RightSide.m_pDialogueStr[i]->ShowComponent(TRUE);
					}

					for (BYTE i = 0; i < BOX_PH_COUNT; ++i)
					{
						if (m_RightSide.m_pExpVal->m_rc.top > 420 || m_RightSide.m_pExpVal->m_rc.top < 100 || !m_pTQUEST)
							m_RightSide.m_pExpVal->ShowComponent(FALSE);
						else
							m_RightSide.m_pExpVal->ShowComponent(TRUE);

						if (!FindKid(m_RightSide.m_pExpBox[i]->m_id) || m_RightSide.m_pExpBox[i]->m_rc.top > 420 || m_RightSide.m_pExpBox[i]->m_rc.top < 100 || !m_pTQUEST)
						{
							m_RightSide.m_pExpBox[i]->ShowComponent(FALSE);
							m_RightSide.m_pExpVal->ShowComponent(FALSE);
						}
						else
							m_RightSide.m_pExpBox[i]->ShowComponent(TRUE);

						if (m_RightSide.m_pSoulVal->m_rc.top > 420 || m_RightSide.m_pSoulVal->m_rc.top < 100 || !m_pTQUEST)
							m_RightSide.m_pSoulVal->ShowComponent(FALSE);
						else
							m_RightSide.m_pSoulVal->ShowComponent(TRUE);

						if (!FindKid(m_RightSide.m_pSoulBox[i]->m_id) || m_RightSide.m_pSoulBox[i]->m_rc.top > 420 || m_RightSide.m_pSoulBox[i]->m_rc.top < 100 || !m_pTQUEST)
						{
							m_RightSide.m_pSoulBox[i]->ShowComponent(FALSE);
							m_RightSide.m_pSoulVal->ShowComponent(FALSE);
						}
						else
							m_RightSide.m_pSoulBox[i]->ShowComponent(TRUE);


						if (m_RightSide.m_pAVVal->m_rc.top > 420 || m_RightSide.m_pAVVal->m_rc.top < 100 || !m_pTQUEST)
							m_RightSide.m_pAVVal->ShowComponent(FALSE);
						else
							m_RightSide.m_pAVVal->ShowComponent(TRUE);

						if (!FindKid(m_RightSide.m_pAVBox[i]->m_id) || m_RightSide.m_pAVBox[i]->m_rc.top > 420 || m_RightSide.m_pAVBox[i]->m_rc.top < 100 || !m_pTQUEST)
						{
							m_RightSide.m_pAVBox[i]->ShowComponent(FALSE);
							m_RightSide.m_pAVVal->ShowComponent(FALSE);
						}
						else
							m_RightSide.m_pAVBox[i]->ShowComponent(TRUE);
					}

					for (BYTE i = 0; i < 3; ++i)
					{
						if (i < 2)
						{
							if (!FindKid(m_RightSide.m_pMoneyBox[i]->m_id) || m_RightSide.m_pMoneyBox[i]->m_rc.top > 420 || m_RightSide.m_pMoneyBox[i]->m_rc.top < 100 || !m_pTQUEST)
								m_RightSide.m_pMoneyBox[i]->ShowComponent(FALSE);
							else
								m_RightSide.m_pMoneyBox[i]->ShowComponent(TRUE);
						}

						if (!FindKid(m_RightSide.m_pMoney[i]->m_id) || m_RightSide.m_pMoney[i]->m_rc.top > 420 || m_RightSide.m_pMoney[i]->m_rc.top < 100 || !m_pTQUEST)
							m_RightSide.m_pMoney[i]->ShowComponent(FALSE);
						else
							m_RightSide.m_pMoney[i]->ShowComponent(TRUE);
					}
				}
			}
		}

		TComponent* pLast = nullptr;
		for (auto& Hide : vHide)
		{
			TComponent* pHide = (TComponent*)Hide;
			if (pHide)
				pHide->ShowComponent(FALSE);
		}

		if (!m_bHideMap)
		{
			RemoveKid(m_pMapBorder);
			RemoveKid(m_pMyCharPos[0]);
			RemoveKid(m_pMyCharPos[1]);
			RemoveKid(m_pMapList);
			RemoveKid(m_pNoTerm);

			AddKid(m_pMapBorder);
			AddKid(m_pMapList);
			AddKid(m_pNoTerm);
			AddKid(m_pMyCharPos[0]);
			AddKid(m_pMyCharPos[1]);

			m_pMapBorder->ShowComponent(TRUE);

			m_pMyCharPos[0]->ShowComponent(TRUE);
			m_pMyCharPos[1]->ShowComponent(TRUE);

			m_pMapList->ShowComponent(TRUE);
			m_pNoTerm->ShowComponent(TRUE);

			for (BYTE i = 0; i < 4; ++i)
				m_pShowWhenMap[i]->ShowComponent(TRUE);

			RederMapPos(dwTickCount);
		}
		else
		{
			m_pMapBorder->ShowComponent(FALSE);

			m_pMyCharPos[0]->ShowComponent(FALSE);
			m_pMyCharPos[1]->ShowComponent(FALSE);

			m_pMapList->ShowComponent(FALSE);
			m_pNoTerm->ShowComponent(FALSE);

			for (BYTE i = 0; i < 4; ++i)
				m_pShowWhenMap[i]->ShowComponent(FALSE);
		}
	}

	/*
	INT nIndex = this->GetSel();

	LPTQUEST pTQUEST = NULL;
	if (nIndex >= 0)
	pTQUEST = this->GetTQUEST(nIndex);

	if (m_pTQUEST != pTQUEST)
	{
	ResetTQUEST(pTQUEST);
	ResetMap(pTQUEST);
	}


	if (m_bPrintMSG)
	ResetButton();

	if (!m_bHideMap)
	{
	RenderMap(dwTickCount);

	if (m_bRenderMap)
	RederMapPos(dwTickCount);
	}


	INT nTermCount = m_pTerm->GetItemCount();
	INT nTermTop = m_pTerm->GetTop();
	INT nTermIndex = nTermCount - nTermTop;

	for (int i = 0; i < TERM_SLOT_COUNT; ++i)
	{
	BOOL bHideSlot = TRUE;

	if (pTQUEST)
	{
	if ((i <= nTermIndex) && (pTQUEST->m_vTTERM.size() > i))
	{
	if (pTQUEST->m_vTTERM[i]->m_bType != QTT_TIMER)
	{
	CTClientQuest *pTQuest = m_pHost->FindTQuest(pTQUEST->m_dwID);
	if (pTQuest)
	{
	CTClientTerm *pTTerm = pTQuest->FindTTerm(pTQUEST->m_vTTERM[i]);
	if (pTTerm)
	switch (pTTerm->GetResult())
	{
	case TTERMRESULT_COMPLETE:
	m_pTermICon[TERM_QUEST_COMPLETED][i]->ShowComponent(TRUE);
	m_pTermICon[TERM_QUEST_INCOMPLETED][i]->ShowComponent(FALSE);
	m_pTermICon[TERM_QUEST_FAIL][i]->ShowComponent(FALSE);
	bHideSlot = FALSE;
	break;
	} // case
	} // if
	}
	}
	}

	if (bHideSlot)
	{
	m_pTermICon[TERM_QUEST_COMPLETED][i]->ShowComponent(FALSE);
	m_pTermICon[TERM_QUEST_INCOMPLETED][i]->ShowComponent(FALSE);
	m_pTermICon[TERM_QUEST_FAIL][i]->ShowComponent(FALSE);
	}

	}

	LPTREWARD pTREWARD[TREWARDITEMCOUNT] = { NULL, NULL };
	nIndex = m_pReward->GetTop();

	for (BYTE i = 0; i<TREWARDITEMCOUNT; i++)
	{
	if (m_pReward->GetItemCount() > 0 && nIndex >= 0)
	{
	DWORD dwValue = m_pReward->GetItemData(nIndex, i);

	if (dwValue == -1)
	pTREWARD[i] = NULL;
	else
	pTREWARD[i] = (LPTREWARD)dwValue;
	}

	if (!pTREWARD[i])
	{
	m_pITEM[i]->ShowComponent(FALSE);
	m_pSKILL[i]->ShowComponent(FALSE);
	}

	if (m_pSEL[i])
	m_pSEL[i]->ShowComponent(FALSE);

	if (m_pTQUEST &&
	m_pTQUEST->m_bType == QT_COMPLETE &&
	!m_bPrintMSG &&
	pTREWARD[i] &&
	pTREWARD[i]->m_bMethod == RM_SELECT &&
	(!m_pTSELREWARD || m_pTSELREWARD == pTREWARD[i]))
	{
	if (m_pSEL[i])
	m_pSEL[i]->ShowComponent(TRUE);

	m_pTSELREWARD = pTREWARD[i];
	}
	}

	if (m_pTTOPREWARD != pTREWARD[0])
	{
	m_pTTOPREWARD = pTREWARD[0];

	for (auto i = 0; i<TREWARDITEMCOUNT; i++)
	{
	m_pSKILL[i]->ShowComponent(FALSE);
	m_pITEM[i]->ShowComponent(FALSE);

	if (pTREWARD[i])
	switch (pTREWARD[i]->m_bType)
	{
	case RT_SKILL:
	case RT_SKILLUP:
	{
	LPTSKILL pTSKILL = CTChart::FindTSKILLTEMP(WORD(pTREWARD[i]->m_dwID));

	if (pTSKILL)
	{
	m_pSKILL[i]->SetCurImage(pTSKILL->m_wIconID);
	m_pSKILL[i]->ShowComponent(TRUE);
	}
	}
	break;

	case RT_ITEM:
	case RT_MAGICITEM:
	{
	LPTITEM pTITEM = NULL;
	if (pTREWARD[i]->m_bType == RT_ITEM)
	pTITEM = CTChart::FindTITEMTEMP((WORD)pTREWARD[i]->m_dwID);
	else
	{
	LPTQUESTITEM pQuestItem = CTChart::FindTQUESTMAGICITEM((WORD)pTREWARD[i]->m_dwID);
	if (pQuestItem)
	pTITEM = CTChart::FindTITEMTEMP(pQuestItem->m_wItemID);
	else
	pTITEM = NULL;
	}

	if (pTITEM)
	{
	if (pTREWARD[i]->m_bCount > 1)
	m_pITEM[i]->m_strText = CTChart::Format(TSTR_FMT_NUMBER, pTREWARD[i]->m_bCount);
	else
	m_pITEM[i]->m_strText.Empty();

	LPTITEMVISUAL pTVISUAL = CTClientItem::GetDefaultVisual(pTITEM);
	WORD wImg = pTVISUAL ? pTVISUAL->m_wIcon : 0;

	m_pITEM[i]->SetCurImage(wImg);
	m_pITEM[i]->ShowComponent(TRUE);
	}
	}
	break;
	}
	}
	}

	if (m_bPrintMSG)
	{
	int nCount = m_pQLIST->GetItemCount();
	int nTop = m_pQLIST->GetTop();
	for (INT i = 0; i <5; ++i)
	{
	int nIndex = nTop + i;
	if (m_pStatusIcon[i])
	m_pStatusIcon[i]->SetCurImage(0);

	if (m_pQLIST->GetItemString(nIndex, COL_STATE).IsEmpty())
	{
	CTClientQuest* pTQuest = GetCQUEST(nIndex);
	if (pTQuest && m_pStatusIcon[i])
	{
	switch (pTQuest->GetResult())
	{
	case TTERMRESULT_COMPLETE: m_pStatusIcon[i]->SetCurImage(1); break;
	case TTERMRESULT_INCOMPLETE: m_pStatusIcon[i]->SetCurImage(2); break;
	case TTERMRESULT_FAILED: m_pStatusIcon[i]->SetCurImage(3); break;
	case TTERMRESULT_TIMEOUT: m_pStatusIcon[i]->SetCurImage(4); break;
	}
	}
	}
	}
	}
	else
	{
	for (INT i = 0; i <5; ++i)
	if (m_pStatusIcon[i])
	m_pStatusIcon[i]->SetCurImage(0);
	}
	}
	*/


	return CTClientUIBase::Render(dwTickCount);
}

void CQuestNewDlg::OnLButtonUp(UINT nFlags, CPoint pt)
{
	for (BYTE i = 0; i < 3; ++i)
	{
		for (auto& QClass : m_QuestList[i].m_mapQuest)
		{
			for (auto& Quest : QClass.second)
			{
				if (Quest.m_pCheckBox->HitTest(pt) && Quest.m_pCheckBox->IsVisible())
				{
					Quest.m_bSelected = !Quest.m_bSelected;
					CheckShowRight(Quest);

					break;
				}

				if (Quest.m_pQuestBtn->HitTest(pt) || Quest.m_pQuestName->HitRect(pt) && Quest.m_pQuestBtn->IsVisible())
				{
					CTClientQuest* pSEL = (CTClientQuest*)Quest.m_dwQuest;
					if (!pSEL)
						continue;

					if (m_Quest != &Quest)
					{
						m_RightSide.Reset();

						if (!Quest.m_Accepted)
						{
							m_pTQUEST = (LPTQUEST)Quest.m_dwQuest;
							ResetTQUEST(m_pTQUEST, &Quest);
						}
						else
						{
							m_pTQUEST = (LPTQUEST)pSEL->m_pTQUEST;
							ResetTQUEST(m_pTQUEST, &Quest);
						}
					}

					break;
				}
			}
		}

		if (m_QuestList[i].m_pQuestCat->HitTest(pt) && m_QuestList[i].GetSize() && m_QuestList[i].m_pQuestCat->IsVisible())
		{
			m_QuestList[i].m_Opened = !m_QuestList[i].m_Opened;
			m_nDesiredLeftScrollY = -1;

			break;
		}

		if (m_pACCEPT->HitTest(pt))
		{
			if (m_pACCEPT->m_strText == ACCEPT)
				CTClientGame::GetInstance()->OnGM_QUEST_ACCEPT();
			else if (m_pACCEPT->m_strText == GIVE_UP)
				CTClientGame::GetInstance()->OnGM_QUEST_GIVEUP();
			else if (m_pACCEPT->m_strText == COMPLETE)
				CTClientGame::GetInstance()->OnGM_QUEST_ACCEPT();
			else if (m_pACCEPT->m_strText == NEXT)
			{
				BOOL CheckNext = FALSE;
				INT nSmallestDif = 255;
				INT nOriY = 0;
				CQuest* NextQuest = nullptr;

				if (m_Quest)
					nOriY = m_Quest->m_pQuestBtn->m_rc.top;

				for (BYTE i = 0; i < 3; ++i)
				{
					for (auto& QClass : m_QuestList[i].m_mapQuest)
					{
						for (auto& Quest : QClass.second)
						{
							if (nOriY && Quest.m_pQuestBtn->m_rc.top > nOriY)
							{
								if (Quest.m_pQuestBtn->m_rc.top - nOriY < nSmallestDif && m_Quest != &Quest)
								{
									nSmallestDif = Quest.m_pQuestBtn->m_rc.top - nOriY;
									NextQuest = &Quest;
								}
							}
						}
					}
				}

				if (NextQuest)
				{
					m_pTQUEST = nullptr;
					m_Quest = nullptr;

					CTClientQuest* pSEL = (CTClientQuest*)NextQuest->m_dwQuest;
					m_RightSide.Reset();

					if (!NextQuest->m_Accepted)
					{
						m_pTQUEST = (LPTQUEST)NextQuest->m_dwQuest;
						ResetTQUEST(m_pTQUEST, NextQuest);
					}
					else
					{
						m_pTQUEST = (LPTQUEST)pSEL->m_pTQUEST;
						ResetTQUEST(m_pTQUEST, NextQuest);
					}

					break;
				}
			}
		}

		if (m_pREFUSE->HitTest(pt))
		{
			if (m_pREFUSE->m_strText == NEXT)
			{
				BOOL CheckNext = FALSE;
				INT nSmallestDif = 255;
				INT nOriY = 0;
				CQuest* NextQuest = nullptr;

				if (m_Quest)
					nOriY = m_Quest->m_pQuestBtn->m_rc.top;

				for (BYTE i = 0; i < 3; ++i)
				{
					for (auto& QClass : m_QuestList[i].m_mapQuest)
					{
						for (auto& Quest : QClass.second)
						{
							if (nOriY && Quest.m_pQuestBtn->m_rc.top > nOriY)
							{
								if (Quest.m_pQuestBtn->m_rc.top - nOriY < nSmallestDif && m_Quest != &Quest)
								{
									nSmallestDif = Quest.m_pQuestBtn->m_rc.top - nOriY;
									NextQuest = &Quest;
								}
							}
						}
					}
				}

				if (NextQuest)
				{
					m_pTQUEST = nullptr;
					m_Quest = nullptr;

					CTClientQuest* pSEL = (CTClientQuest*)NextQuest->m_dwQuest;
					m_RightSide.Reset();

					if (!NextQuest->m_Accepted)
					{
						m_pTQUEST = (LPTQUEST)NextQuest->m_dwQuest;
						ResetTQUEST(m_pTQUEST, NextQuest);
					}
					else
					{
						m_pTQUEST = (LPTQUEST)pSEL->m_pTQUEST;
						ResetTQUEST(m_pTQUEST, NextQuest);
					}

					break;
				}
			}
			else if (m_pREFUSE->m_strText == RUN)
			{
				CTGaugePannel *pTPANNEL = (CTGaugePannel *)CTClientGame::GetInstance()->GetFrame(TFRAME_GAUGE);
				if (m_pTQUEST)
				{
					pTPANNEL->m_dwCurQuestID = m_pTQUEST->m_dwID;
					pTPANNEL->m_bAutoPath = TRUE;
					pTPANNEL->m_bShowPath = TRUE;
					pTPANNEL->UpdateQuestBUTTON();
				}
			}
		}
	}

	CTClientUIBase::OnLButtonUp(nFlags, pt);
}

void CQuestNewDlg::OnLButtonDown(UINT nFlags, CPoint pt)
{
	CTClientUIBase::OnLButtonDown(nFlags, pt);
}

void CQuestNewDlg::OnRButtonUp(UINT nFlags, CPoint pt)
{
	return CTClientUIBase::OnRButtonUp(nFlags, pt);
}

void CQuestNewDlg::OnRButtonDown(UINT nFlags, CPoint pt)
{
	return CTClientUIBase::OnRButtonDown(nFlags, pt);
}

void CQuestNewDlg::OnMouseMove(UINT nFlags, CPoint pt)
{
	if (m_pShowMap->HitTest(pt))
		m_bHideMap = FALSE;
	else
		m_bHideMap = TRUE;

	CTClientUIBase::OnMouseMove(nFlags, pt);
}

void CQuestNewDlg::ShowComponent(BOOL bVisible)
{
	MoveComponent(CQuestNewDlg::m_ptPOS);
	CTClientUIBase::ShowComponent(bVisible);

	m_pCheckBox->ShowComponent(FALSE);
	m_pQuestBtn->ShowComponent(FALSE);
	m_pQuestName->ShowComponent(FALSE);
	m_pProgress->ShowComponent(FALSE);
	FindKid(26129)->ShowComponent(FALSE);
	FindKid(26130)->ShowComponent(FALSE);
	m_pQuestCatReg->ShowComponent(FALSE);
	FindKid(26564)->ShowComponent(FALSE);
	FindKid(26152)->ShowComponent(FALSE);

	static_cast<TImageList*>(FindKid(26141))->ShowComponent(FALSE);

	m_nCurLeftScrollY = 0;
	m_nDesiredLeftScrollY = 0;

	return;
	/*

	if (bVisible)
	{
	for (int i = 0; i < GetCount(); ++i)
	{
	LPTQUEST pQUEST = GetTQUEST(i);
	if (pQUEST)
	{
	SetCurSel(i);
	break;
	}
	}

	ResetTQUEST(m_pTQUEST);
	ResetMap(m_pTQUEST);

	for (INT i = 0; i<m_nGBTNCOUNT; ++i)
	{
	m_vTCOLLAPSE[i]->ShowComponent(FALSE);
	m_vTEXPEND[i]->ShowComponent(FALSE);
	m_vTCHECK[i]->ShowComponent(FALSE);
	}

	if (m_bHideMap)
	{
	m_pMapOpen->ShowComponent(TRUE);
	m_pMapFrame->ShowComponent(FALSE);
	m_pMapClose->ShowComponent(FALSE);

	}
	else
	{
	m_pMapOpen->ShowComponent(FALSE);
	m_pMapFrame->ShowComponent(TRUE);
	m_pMapClose->ShowComponent(TRUE);
	}
	}
	else
	{
	if (m_pBACKBUF2)
	{
	m_pBACKBUF2->Release();
	m_pBACKBUF2 = NULL;
	}

	if (m_pBACKTEX2)
	{
	m_pBACKTEX2->Release();
	m_pBACKTEX2 = NULL;
	}
	}
	*/
}

ITDetailInfoPtr CQuestNewDlg::GetTInfoKey(const CPoint& point)
{
	ITDetailInfoPtr pInfo;

	for (BYTE i = 0; i<m_RightSide.m_RewardCnt; i++)
	{
		if (!m_RightSide.m_pRewardName[i]->GetParam(0))
			continue;

		if (!m_RightSide.m_pItemIcon[i]->HitTest(point) && !m_RightSide.m_pSkillIcon[i]->HitTest(point))
			continue;

		LPTREWARD pTREWARD = (LPTREWARD)m_RightSide.m_pRewardName[i]->GetParam(0);
		if (!pTREWARD)
			continue;

		if (pTREWARD && (DWORD)(pTREWARD) != (DWORD)(-1))
		{
			switch (pTREWARD->m_bType)
			{
			case RT_SKILL:
			case RT_SKILLUP:
			{
				LPTSKILL pTSKILL = CTChart::FindTSKILLTEMP(WORD(pTREWARD->m_dwID));

				if (pTSKILL)
				{
					CRect rc;
					GetComponentRect(&rc);

					if (pTREWARD->m_bType == RT_SKILL)
					{
						pInfo = CTDetailInfoManager::NewSkillInst(
							pTSKILL, 1, FALSE, rc);
					}
					else if (pTREWARD->m_bType == RT_SKILLUP)
					{
						pInfo = CTDetailInfoManager::NewSkillInst(
							pTSKILL, pTREWARD->m_bCount, FALSE, rc);
					}

					CPoint pt;
					m_RightSide.m_pItemFrame[i]->GetComponentPos(&pt);
					m_RightSide.m_pItemFrame[i]->ComponentToScreen(&pt);

					pInfo->SetDir(TRUE, TRUE, TRUE, TRUE, pt);
				}
			}
			break;

			case RT_ITEM:
			case RT_MAGICITEM:
			{
				CRect rc;
				GetComponentRect(&rc);

				if (pTREWARD->m_bType == RT_ITEM)
				{
					LPTITEM pTITEM = CTChart::FindTITEMTEMP(WORD(pTREWARD->m_dwID));
					pInfo = CTDetailInfoManager::NewItemInst(pTITEM, rc);
				}
				else
				{
					CTClientItem* pCItem = CTClientItem::FindTQuestMagicClientItem((WORD)pTREWARD->m_dwID);
					pInfo = CTDetailInfoManager::NewItemInst(pCItem, rc);
				}

				CPoint pt;
				m_RightSide.m_pItemIcon[i]->GetComponentPos(&pt);
				m_RightSide.m_pItemIcon[i]->ComponentToScreen(&pt);

				pInfo->SetDir(TRUE, TRUE, TRUE, TRUE, pt);
			}
			}
			break;
		}

	}

	return pInfo;
}

BOOL CQuestNewDlg::GetTChatInfo(const CPoint& point, TCHATINFO& outInfo)
{
	return FALSE;
}

void CQuestNewDlg::Reset()
{
	MoveComponent(CPoint(CTClientUIBase::m_vBasis[TBASISPOINT_CENTER_MIDDLE].x - 384, CTClientUIBase::m_vBasis[TBASISPOINT_CENTER_MIDDLE].y - 250));
}

void CQuestNewDlg::ResetTQUEST(LPTQUEST pTQUEST, CQuest* Quest)
{
	if (m_Quest == Quest)
		return;

	if (!Quest)
		return;

	BOOL IsRemote = FALSE;
	IsRemote = this != CTClientGame::GetInstance()->GetFrame(TFRAME_QUEST_NEWUI);

	m_nCurRightScrollY = 0;
	m_nDesiredRightScrollY = 0;

	LPTQUEST pTMISSION = pTQUEST;
	if (!pTMISSION)
		return;

	if (pTMISSION->m_bType == QT_COMPLETE)
		pTMISSION = CTChart::FindTMISSION(pTMISSION);

	if (!pTMISSION)
		return;

	m_pTQUEST = pTMISSION;

	BOOL bCanPerf = TRUE;

	if (m_pTQUEST && Quest->m_Completed)
	{
		m_pACCEPT->m_strText = CTChart::LoadString((TSTRING)m_dwCompleteID);
	}
	else if (m_pTQUEST && m_pTQUEST->m_bType == QT_MISSION)
	{
		CTClientGame* pTGAME = CTClientGame::GetInstance();
		CTClientObjBase* pTARGET = pTGAME->GetTargetObj();
		if (pTARGET)
		{
			MAPDWORD::iterator findCOND = pTARGET->m_mapTQUESTCOND.find(m_pTQUEST->m_dwID);
			if (findCOND != pTARGET->m_mapTQUESTCOND.end())
			{
				if (findCOND->second != QCT_NONE)
					bCanPerf = FALSE;
			}
		}

		m_pACCEPT->m_strText = CTChart::LoadString((TSTRING)m_dwAcceptID);
	}
	else
	{
		m_pACCEPT->m_strText = CTChart::LoadString((TSTRING)m_dwAcceptID);

		if (m_pTQUEST && bCanPerf)
			m_pACCEPT->EnableComponent(TRUE);
		else
			m_pACCEPT->EnableComponent(FALSE);
	}

	if (!IsRemote && Quest->m_Accepted)
	{
		m_pACCEPT->m_strText = GIVE_UP;
		m_pREFUSE->m_strText = RUN;
	}
	if (IsRemote && !Quest->m_Completed)
	{
		m_pACCEPT->m_strText = ACCEPT;
		m_pREFUSE->m_strText = NEXT;
	}
	if (!IsRemote && !Quest->m_Accepted)
	{
		m_pACCEPT->m_strText = GIVE_UP;
		m_pREFUSE->m_strText = RUN;
	}
	if (IsRemote && Quest->m_Completed)
	{
		m_pACCEPT->m_strText = COMPLETE;
		m_pREFUSE->m_strText = RUN;
	}


	INT nIndex = 0;
	if (pTMISSION)
	{
		m_Quest = Quest;
		m_RightSide.Reset();
		m_pQuestText->m_strText = pTMISSION->m_strTITLE;

		ResetMap(pTMISSION);

		for (auto& pReward : pTMISSION->m_vTREWARD)
		{
			switch (pReward->m_bType)
			{
			case RT_ITEM:
			{
				LPTITEM pITEM = CTChart::FindTITEMTEMP((WORD)pReward->m_dwID);
				if (pITEM && !TCHECK_CLASS(pITEM->m_dwClassID, m_pHost->m_bClassID))
					continue;
			}
			break;
			case RT_MAGICITEM:
			{
				LPTQUESTITEM pQITEM = CTChart::FindTQUESTMAGICITEM((WORD)pReward->m_dwID);
				if (!pQITEM)
					continue;

				LPTITEM pITEM = CTChart::FindTITEMTEMP(pQITEM->m_wItemID);
				if (pITEM && !TCHECK_CLASS(pITEM->m_dwClassID, m_pHost->m_bClassID))
					continue;
			}
			break;
			case RT_SKILL:
			case RT_SKILLUP:
			{
				LPTSKILL pSKILL = CTChart::FindTSKILLTEMP((WORD)pReward->m_dwID);
				if (pSKILL && !TCHECK_CLASS(pSKILL->m_dwClassID, m_pHost->m_bClassID))
					continue;
			}
			break;
			}

			m_RightSide.AddReward(pReward->m_bType, pReward->m_dwID, pReward->m_bCount, pReward, pTMISSION->m_dwID);
		}

		for (const auto& Term : pTMISSION->m_vTTERM)
		{
			BYTE bCount = 0;
			CTClientQuest* pTQUEST = m_pHost->FindTQuest(pTMISSION->m_dwID);
			if (pTQUEST)
			{
				if (pTQUEST->FindTTerm(Term))
					bCount = pTQUEST->FindTTerm(Term)->m_bCount;
			}

			m_RightSide.AddTerm(Term, bCount);
		}

		m_RightSide.SummaryMsg(pTMISSION->m_strSummaryMSG);

		CString strHeader = GetSpeakerString(m_strNPCTitle);
		m_RightSide.DialogueMsg(strHeader, pTMISSION->m_strTriggerMSG);

		strHeader = GetSpeakerString(m_pHost->GetName());
		m_RightSide.DialogueMsg(strHeader, pTMISSION->m_strReply);

		m_RightSide.ReAlign(0);
	}

	/*

	int nIndex = 0;
	if (m_pTQUEST)
	{
	m_strNPCTitle = m_pTQUEST->m_strNPCName;

	if (!bCanPerf)
	{
	// ÇŇĽöľř´ÂÄů˝şĆ®
	CString strCANNOTMSG;
	strCANNOTMSG = CTChart::LoadString(TSTR_QUEST_CANNOTPERF);

	SummaryMessage(strCANNOTMSG);
	}
	else
	{
	// ¸ńÇĄÁ¤ş¸
	INT nTermLine = 0;
	INT nNumber = 1;
	for (BYTE i = 0; i<INT(pTMISSION->m_vTTERM.size()); i++)
	{
	if (pTMISSION->m_vTTERM[i]->m_bType != QTT_TIMER)
	{
	CString strRESULT;

	CTClientQuest *pTQuest = m_pHost->FindTQuest(pTMISSION->m_dwID);
	if (pTQuest)
	{
	CTClientTerm *pTTerm = pTQuest->FindTTerm(pTMISSION->m_vTTERM[i]);
	if (pTTerm)
	switch (pTTerm->GetResult())
	{
	case TTERMRESULT_INCOMPLETE:
	strRESULT = CTChart::Format(TSTR_QUEST_STATUS, pTTerm->m_bCount, pTMISSION->m_vTTERM[i]->m_bCount);
	break;

	case TTERMRESULT_TIMEOUT: strRESULT.Empty(); break; //  = CTChart::LoadString( TSTR_QRESULT_TIMEOUT); break;
	case TTERMRESULT_FAILED: strRESULT.Empty(); break; // = CTChart::LoadString( TSTR_QRESULT_FAILED); break;
	case TTERMRESULT_COMPLETE: strRESULT.Empty(); break; //  = CTChart::LoadString( TSTR_QRESULT_COMPLETE); break;
	}
	else
	{
	strRESULT = CTChart::Format(TSTR_QUEST_STATUS, 0, pTMISSION->m_vTTERM[i]->m_bCount);
	}
	}

	TermMessage(
	pTMISSION->m_vTTERM[i]->m_dwID,
	pTMISSION->m_vTTERM[i]->m_strTermMSG,
	strRESULT,
	nTermLine,
	nNumber);
	}
	}

	// żäľŕÁ¤ş¸
	SummaryMessage(pTMISSION->m_strSummaryMSG);

	// ´ëČ­ł»żë
	if (m_bPrintMSG)
	{
	CString strHeader = GetSpeakerString(m_strNPCTitle);
	TextMessage(strHeader, pTMISSION->m_strTriggerMSG);

	strHeader = GetSpeakerString(m_pHost->GetName());
	TextMessage(strHeader, pTMISSION->m_strReply);

	strHeader = GetSpeakerString(m_strNPCTitle);
	TextMessage(strHeader, pTMISSION->m_strAcceptMSG);
	}

	INT nRewardItemCount = 0;

	// ş¸»ó¸ń·Ď
	for (auto i = 0; i<INT(pTMISSION->m_vTREWARD.size()); i++)
	{
	CString strREWARD;
	strREWARD.Empty();

	switch (pTMISSION->m_vTREWARD[i]->m_bType)
	{
	case RT_SKILL:
	case RT_SKILLUP:
	{
	LPTSKILL pTSKILL = CTChart::FindTSKILLTEMP(WORD(pTMISSION->m_vTREWARD[i]->m_dwID));
	CTClientSkill* pTHOSTSKILL = m_pHost->FindTSkill(WORD(pTMISSION->m_vTREWARD[i]->m_dwID));

	/*BOOL bHasAlready = (pTHOSTSKILL) &&
	(pTSKILL->m_dwClassID == pTHOSTSKILL->m_pTSKILL->m_dwClassID) &&
	(pTSKILL->m_bLevel == pTHOSTSKILL->m_pTSKILL->m_bLevel);

	if (pTSKILL &&
	TCHECK_CLASS(pTSKILL->m_dwClassID, m_pHost->m_bClassID) /*&&
	!bHasAlready)
	{
	strREWARD = CTChart::Format(TSTR_FMT_SKILL_REWARD, pTSKILL->m_strNAME);
	}
	}

	break;

	case RT_ITEM:
	case RT_MAGICITEM:
	{
	LPTITEM pTITEM = NULL;
	if (pTMISSION->m_vTREWARD[i]->m_bType == RT_ITEM)
	pTITEM = CTChart::FindTITEMTEMP((WORD)pTMISSION->m_vTREWARD[i]->m_dwID);
	else
	{
	LPTQUESTITEM pQuestItem = CTChart::FindTQUESTMAGICITEM((WORD)pTMISSION->m_vTREWARD[i]->m_dwID);
	if (pQuestItem)
	pTITEM = CTChart::FindTITEMTEMP(pQuestItem->m_wItemID);
	else
	pTITEM = NULL;
	}

	if (pTITEM && TCHECK_CLASS(pTITEM->m_dwClassID, m_pHost->m_bClassID))
	{
	strREWARD = CTChart::Format(TSTR_FMT_REWARD, pTITEM->m_strNAME, pTMISSION->m_vTREWARD[i]->m_bCount);
	}
	}

	break;

	case RT_GOLD:
	{
	//strREWARD = CTClientGame::MakeMoneyStr(pTMISSION->m_vTREWARD[i]->m_dwID);
	// ş¸»ó¸ń·ĎŔĚ ľĆ´Ď¶ó µű·Î łÖľîÁÖ´ÂµĄ°ˇ ŔÖ´Ů.

	DWORD dwMONEY[3] = { 0, 0, 0 };

	CTClientGame::SplitMoney(
	pTMISSION->m_vTREWARD[i]->m_dwID,
	&dwMONEY[0],
	&dwMONEY[1],
	&dwMONEY[2]);

	CString strFMT;
	for (INT i = 0; i < 3; ++i)
	{
	strFMT.Format("%u", dwMONEY[i]);
	m_pMONEY[i]->m_strText = strFMT;
	}
	}

	break;

	case RT_EXP:
	{
	CString strEXP;
	strEXP.Format("%u", pTMISSION->m_vTREWARD[i]->m_dwID);
	m_pEXP->m_strText = strEXP;
	}

	break;
	}

	if (!strREWARD.IsEmpty())
	{
	switch (pTMISSION->m_vTREWARD[i]->m_bMethod)
	{
	case RM_SELECT:
	{
	CString strTEXT;

	strTEXT = CTChart::LoadString(TSTR_TEXT_SELREWARD);
	strREWARD += strTEXT;
	}

	break;

	case RM_PROB:
	{
	CString strTEXT;

	strTEXT = CTChart::Format(TSTR_FMT_PROBREWARD, pTMISSION->m_vTREWARD[i]->m_bProb, '%');
	strREWARD += strTEXT;
	}

	break;
	}

	INT nLine = 0;
	INT nCol = 0;

	if ((nRewardItemCount % 2) == 0)
	nLine = m_pReward->AddString(strREWARD);
	else
	{
	nLine = m_pReward->GetItemCount() - 1; // ¸¶Áö¸· ¶óŔÎ
	nCol = 1;

	m_pReward->SetItemString(nLine, nCol, (LPCTSTR)strREWARD);
	}

	m_pReward->SetItemData(nLine, nCol, (DWORD)pTMISSION->m_vTREWARD[i]);
	++nRewardItemCount;
	}
	}
	}

	m_pTTOPREWARD = NULL;
	m_pTSELREWARD = NULL;
	}

	if (m_bPrintMSG == FALSE) // NPC´ëČ­Ă˘ŔĚ¸é
	{
	if (m_strAnswerWhenNPCTalk.GetLength())
	{
	CString strHeader = GetSpeakerString(m_pHost->GetName());
	TextMessage(strHeader, m_strAnswerWhenNPCTalk);
	}

	if (m_strNPCTalk.GetLength())
	{
	CString strHeader = GetSpeakerString(m_strNPCTitle);
	TextMessage(strHeader, m_strNPCTalk);
	}
	}
	*/

}

void CQuestNewDlg::SummaryMessage(const CString& strText)
{
}

void CQuestNewDlg::TermMessage(DWORD dwID, const CString& strText, const CString& strRESULT, INT nLine, INT& nNumber)
{

}

void CQuestNewDlg::TextMessage(CString strTitle, CString strText)
{

}

void CQuestNewDlg::TextMessage(DWORD dwTitleID, CString strText)
{
}

CString CQuestNewDlg::GetSpeakerString(CString strSpeaker)
{
	CString strResult(_T(""));

	strSpeaker.TrimRight();
	strSpeaker.TrimLeft();

	if (!strSpeaker.IsEmpty())
		strResult = CTChart::Format(TSTR_FMT_QUEST_SPEEKER, strSpeaker);

	return strResult;
}

CString CQuestNewDlg::GetResultString(CTClientQuest* pTQUEST)
{
	BYTE bResult = pTQUEST->GetResult();
	CString strRESULT;
	strRESULT.Empty();

	if (bResult != TTERMRESULT_FAILED && bResult != TTERMRESULT_TIMEOUT && pTQUEST->m_bTimmer)
	{
		strRESULT = CTChart::Format(TSTR_FMT_TIMMER, (pTQUEST->m_dwTick % 3600000) / 60000, (pTQUEST->m_dwTick % 60000) / 1000);
		return strRESULT;
	}

	switch (bResult)
	{
	case TTERMRESULT_INCOMPLETE: strRESULT.Empty(); break; // = CTChart::LoadString( TSTR_QRESULT_INCOMPLETE); break;
	case TTERMRESULT_TIMEOUT: strRESULT.Empty(); break; // = CTChart::LoadString( TSTR_QRESULT_TIMEOUT); break;
	case TTERMRESULT_FAILED: strRESULT.Empty(); break; // = CTChart::LoadString( TSTR_QRESULT_FAILED); break;
	case TTERMRESULT_COMPLETE: strRESULT.Empty(); break; // = CTChart::LoadString( TSTR_QRESULT_COMPLETE); break;
	}

	return strRESULT;
}

void CQuestNewDlg::CheckShowRight(const CQuest& Quest)
{
	CTClientWnd *pMainWnd = (CTClientWnd *)AfxGetMainWnd();
	if (!Quest.m_Accepted)
		return;

	if (pMainWnd)
	{
		CTClientQuest *pCQuest = (CTClientQuest*)Quest.m_dwQuest;

		if (pCQuest)
		{
			CTGaugePannel *pTPANNEL = (CTGaugePannel *)pMainWnd->m_MainGame.GetFrame(TFRAME_GAUGE);

			pCQuest->m_bCheckShowRight = Quest.m_bSelected;
			if (pTPANNEL)
				pTPANNEL->UpdateQuestINFO(pCQuest);
		}
	}
}

// ----------------------------------------------------------------------------------------------------
void CQuestNewDlg::ResetTree(BYTE FindNew)
{
	std::map<DWORD, std::vector<CTClientQuest*>> vQuests[3];
	LISTTQCLASS vTQCLASS;

	m_pTQUEST = nullptr;
	m_Quest = nullptr;
	m_pQuestText->m_strText.Empty();
	m_RightSide.m_pDialogueDot->ShowComponent(FALSE);
	m_RightSide.m_pDialogueText->ShowComponent(FALSE);
	m_RightSide.Reset();

	for (BYTE i = 0; i < 3; ++i)
	{
		m_QuestList[i].DeleteQuestList();
		vQuests[i].clear();
	}

	MAPTQCLASS::iterator itr = m_pHost->m_mapTQCLASS.begin();
	MAPTQCLASS::iterator end = m_pHost->m_mapTQCLASS.end();

	for (; itr != end; ++itr)
		vTQCLASS.push_back(itr->second);
	vTQCLASS.sort(binary_qclass());

	while (!vTQCLASS.empty())
	{
		CTClientQClass* pTCLASS = vTQCLASS.front();

		LISTTQUEST vTQUEST;

		MAPTQUEST::iterator itQUEST, endQUEST;
		itQUEST = pTCLASS->m_mapTQUEST.begin();
		endQUEST = pTCLASS->m_mapTQUEST.end();

		for (; itQUEST != endQUEST; ++itQUEST)
			vTQUEST.push_back(itQUEST->second);

		vTQUEST.sort(binary_tquest());

		while (!vTQUEST.empty())
		{
			CTClientQuest* pTQUEST = vTQUEST.front();
			if (!pTQUEST || !pTQUEST->m_pTQUEST)
				continue;

			BYTE bID = pTQUEST->m_pTQUEST->m_bIsDaily == 1 ? 0 : 1;
			if (!pTCLASS || !pTCLASS->m_pTQCLASS)
				continue;

			std::map<DWORD, std::vector<CTClientQuest*>>::iterator finder = vQuests[bID].find(pTCLASS->m_pTQCLASS->m_dwClassID);
			if (finder == vQuests[bID].end())
			{
				std::vector<CTClientQuest*> vQuest = { pTQUEST };
				vQuests[bID].insert(std::map<DWORD, std::vector<CTClientQuest*>>::value_type(pTCLASS->m_pTQCLASS->m_dwClassID, vQuest));
			}
			else
				finder->second.push_back(pTQUEST);

			vTQUEST.pop_front();
		}

		vTQCLASS.pop_front();
	}

	for (BYTE i = 0; i < 3; ++i)
	{
		m_QuestList[i].ReInit();
		for (auto Q : vQuests[i])
		{
			m_QuestList[i].AddCategory(Q.first, CTChart::FindTQCLASS(Q.first)->m_strNAME, 128);
			for (BYTE q = 0; q < Q.second.size(); ++q)
			{
				CString strLevel; strLevel.Format("%d", Q.second[q]->m_pTQUEST->m_bLevel);
				m_QuestList[i].AddQuest(Q.first, Q.second[q]->m_pTQUEST->m_strTITLE, strLevel, (DWORD_PTR)Q.second[q], TRUE, FALSE);
			}
		}
	}

	switch (FindNew)
	{
	case 0:
	{
		if (!m_pTQUEST)
			ResetTree(2);
	}
	break;
	case 1:
	{
		for (BYTE i = 0; i < 3; ++i)
		{
			if (m_QuestList[i].GetSize() && m_QuestList[i].m_mapQuest.size() && m_QuestList[i].m_mapQuest.begin()->second.size())
			{
				if (m_QuestList[i].m_mapQuest.begin()->second.begin()->m_Accepted)
				{
					CTClientQuest* pTQUEST = (CTClientQuest*)m_QuestList[i].m_mapQuest.begin()->second.begin()->m_dwQuest;
					if (pTQUEST && pTQUEST->m_pTQUEST)
					{
						m_pTQUEST = pTQUEST->m_pTQUEST;
						ResetTQUEST(m_pTQUEST, &m_QuestList[i].m_mapQuest.begin()->second[0]);
					}
				}
				else
				{
					m_pTQUEST = (LPTQUEST)m_QuestList[i].m_mapQuest.begin()->second[0].m_dwQuest;
					ResetTQUEST(m_pTQUEST, &m_QuestList[i].m_mapQuest.begin()->second[0]);
				}

				break;
			}
		}
	}
	break;
	case 2:
	{
		BOOL NextIndex = FALSE;
		for (BYTE i = 0; i < 3; ++i)
		{
			for (auto& QClass : m_QuestList[i].m_mapQuest)
			{
				for (auto& Quest : QClass.second)
				{
					if (NextIndex)
					{
						if (Quest.m_Accepted)
						{
							CTClientQuest* pTQUEST = (CTClientQuest*)Quest.m_dwQuest;
							if (pTQUEST && pTQUEST->m_pTQUEST)
							{
								m_pTQUEST = pTQUEST->m_pTQUEST;
								ResetTQUEST(m_pTQUEST, &Quest);
							}
						}
						else
						{
							m_pTQUEST = (LPTQUEST)Quest.m_dwQuest;
							ResetTQUEST(m_pTQUEST, &Quest);
						}
					}

					if (&Quest == m_Quest)
						NextIndex = TRUE;
				}
			}
		}
		if (!NextIndex)
			ResetTree(1);
	}
	break;
	}
}

// ----------------------------------------------------------------------------------------------------

void CQuestNewDlg::SetQuestState(INT nIdx, LPTQUEST pTQUEST)
{
	CTClientQuest *pTQuest = m_pHost->FindTQuest(pTQUEST->m_dwID);

	if (pTQuest && pTQuest->m_bTimmer)
	{
		CTClientWnd *pMainWnd = (CTClientWnd *)AfxGetMainWnd();
		CTGaugePannel *pTPANNEL = (CTGaugePannel *)pMainWnd->m_MainGame.GetFrame(TFRAME_GAUGE);
		pTPANNEL->UpdateQuestINFO(pTQuest);
	}
}
// ====================================================================================================
CQuest* CQuestNewDlg::GetTQUEST(INT nIdx)
{
	INT Index = 0;
	for (BYTE i = 0; i < 3; ++i)
	{
		for (auto& QClass : m_QuestList[i].m_mapQuest)
		{
			for (auto& Quest : QClass.second)
			{
				if (Index == nIdx)
					return &Quest;

				Index++;
			}
		}
	}

	return nullptr;
}

void CQuestNewDlg::MoveComponent(CPoint pt)
{
	if (pt.x == 0 && pt.y == 0)
		return;

	return CTClientUIBase::MoveComponent(pt);
}

BOOL CQuestNewDlg::DoMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (!IsVisible())
		return FALSE;

	CPoint UIPt, RealPt;
	GetComponentPos(&UIPt);

	RealPt = CPoint(pt.x - UIPt.x, pt.y - UIPt.y);
	if (RealPt.x > 0 && RealPt.x < 340 && RealPt.y > 35 && RealPt.y < 430)
		m_nDesiredLeftScrollY += zDelta / 4;
	else if (RealPt.x < 760 && RealPt.x > 340 && RealPt.y > 35 && RealPt.y < 430)
	{
		if (!m_RightSide.m_Reward && !m_RightSide.m_Goal && !m_RightSide.m_Summary)
			return FALSE;

		m_nDesiredRightScrollY += zDelta / 4;
	}
	else
		return FALSE;

	return TRUE;
}

void CQuestNewDlg::GetUnitPosition(FLOAT fPosX, FLOAT fPosY, INT& outUnitX, INT& outUnitY)
{
	FLOAT fLength = ((FLOAT)m_pTMAP->m_pMAP->m_nUnitLength * m_pTMAP->m_pMAP->m_nTileLength);

	outUnitX = INT(fPosX / fLength);
	outUnitY = INT(fPosY / fLength);
}

void CQuestNewDlg::ResetMap(LPTQUEST pTQUEST)
{
	LPTQUEST pTMISSION = pTQUEST;
	BOOL bFindInCompleteQuest = FALSE;

	m_bRenderMap = FALSE;

	if (pTQUEST && pTQUEST->m_bType == QT_COMPLETE)
		pTMISSION = CTChart::FindTMISSION(m_pTQUEST);

	if (pTQUEST)
	{
		CTClientQuest *pTQuest = m_pHost->FindTQuest(pTQUEST->m_dwID);

		if (pTQuest)
		{

			for (auto Term : pTMISSION->m_vTTERMPOS)
			{
				if (Term->m_bType == 29)
				{
					m_pNoTerm->SetCurImage(0);
					m_pMapList->SetCurImage(-1);

					return;
				}
			}

			for (int i = 0; i<INT(pTMISSION->m_vTTERM.size()); i++)
			{
				CTClientTerm *pTTERM = pTQuest->FindTTerm(pTMISSION->m_vTTERM[i]);

				if (pTTERM && pTTERM->GetResult() == TTERMRESULT_INCOMPLETE)
				{
					for (int j = 0; j<INT(pTMISSION->m_vTTERMPOS.size()); j++)
					{
						LPTTERMPOS pTERMPOS = m_pTQUEST->m_vTTERMPOS[j];

						if (pTMISSION->m_vTTERMPOS[j]->m_bType == pTTERM->m_pTTERM->m_bType &&
							pTMISSION->m_vTTERMPOS[j]->m_dwID == pTTERM->m_pTTERM->m_dwID)
						{
							INT nUnitX, nUnitZ;
							GetUnitPosition(pTERMPOS->m_fPosX, pTERMPOS->m_fPosZ, nUnitX, nUnitZ);
							DWORD dwUNITID = MakeUnitID(WORD(pTERMPOS->m_dwMapID), (BYTE)nUnitX, (BYTE)nUnitZ);
							m_ptMapIcon = D3DXVECTOR3(pTERMPOS->m_fPosX, pTERMPOS->m_fPosY, pTERMPOS->m_fPosZ);
							m_wMapID = (WORD)pTERMPOS->m_dwMapID;

							bFindInCompleteQuest = TRUE;

							MAPTMINIMAP::iterator finder = CTChart::m_mapTMINIMAP.find(dwUNITID);

							if (finder != CTChart::m_mapTMINIMAP.end())
							{
								LPTMINIMAP temp = (*finder).second;
								m_pMapList->SetCurImage((*finder).second->m_bWorldID);
								m_pNoTerm->SetCurImage(-1);
								break;
							}
							break;
						}
					}
				}
			}
		}
		else
		{
			for (int i = 0; i < pTMISSION->m_vTTERM.size(); ++i)
			{
				LPTTERM pTTERM = pTMISSION->m_vTTERM[i];

				for (int j = 0; j<INT(pTMISSION->m_vTTERMPOS.size()); j++)
				{
					LPTTERMPOS pTERMPOS = pTMISSION->m_vTTERMPOS[j];

					if (pTMISSION->m_vTTERMPOS[j]->m_bType == pTTERM->m_bType &&
						pTMISSION->m_vTTERMPOS[j]->m_dwID == pTTERM->m_dwID)
					{
						INT nUnitX, nUnitZ;
						GetUnitPosition(pTERMPOS->m_fPosX, pTERMPOS->m_fPosZ, nUnitX, nUnitZ);
						DWORD dwUNITID = MakeUnitID(WORD(pTERMPOS->m_dwMapID), (BYTE)nUnitX, (BYTE)nUnitZ);
						m_ptMapIcon = D3DXVECTOR3(pTERMPOS->m_fPosX, pTERMPOS->m_fPosY, pTERMPOS->m_fPosZ);
						m_wMapID = (WORD)pTERMPOS->m_dwMapID;

						bFindInCompleteQuest = TRUE;

						MAPTMINIMAP::iterator finder = CTChart::m_mapTMINIMAP.find(dwUNITID);

						if (finder != CTChart::m_mapTMINIMAP.end())
						{
							LPTMINIMAP temp = (*finder).second;
							m_pMapList->SetCurImage((*finder).second->m_bWorldID);
							m_pNoTerm->SetCurImage(-1);
							break;
						}
						break;
					}
				}
			}
		}

		if (!bFindInCompleteQuest)
		{
			for (int i = 0; i<INT(pTMISSION->m_vTTERMPOS.size()); i++)
			{
				LPTTERMPOS pTERMPOS = pTMISSION->m_vTTERMPOS[i];

				if (pTERMPOS->m_bType == QTT_TCOMP_POS)
				{
					INT nUnitX, nUnitZ;
					GetUnitPosition(pTERMPOS->m_fPosX, pTERMPOS->m_fPosZ, nUnitX, nUnitZ);
					DWORD dwUNITID = MakeUnitID(WORD(pTERMPOS->m_dwMapID), (BYTE)nUnitX, (BYTE)nUnitZ);
					m_ptMapIcon = D3DXVECTOR3(pTERMPOS->m_fPosX, pTERMPOS->m_fPosY, pTERMPOS->m_fPosZ);
					m_wMapID = (WORD)pTERMPOS->m_dwMapID;

					bFindInCompleteQuest = TRUE;

					MAPTMINIMAP::iterator finder = CTChart::m_mapTMINIMAP.find(dwUNITID);

					if (finder != CTChart::m_mapTMINIMAP.end())
					{
						LPTMINIMAP temp = (*finder).second;
						m_pMapList->SetCurImage((*finder).second->m_bWorldID);
						m_pNoTerm->SetCurImage(-1);
						break;
					}
					break;
				}
			}
		}
	}
	else
	{
		m_pMapList->SetCurImage(-1);
		m_pNoTerm->SetCurImage(0);
	}
}

void CQuestNewDlg::RederMapPos(DWORD dwTickCount)
{
	if (IsVisible())
	{
		if (m_pNoTerm->GetCurImage() == -1)
		{
			INT nMainUnitX, nMainUnitZ;
			INT nTarUnitX, nTarUnitZ;

			GetUnitPosition(m_pHost->GetPositionX(), m_pHost->GetPositionZ(), nMainUnitX, nMainUnitZ);
			GetUnitPosition(m_ptMapIcon.x, m_ptMapIcon.z, nTarUnitX, nTarUnitZ);

			if (nMainUnitX == nTarUnitX && nMainUnitZ == nTarUnitZ && CTClientGame::GetInstance()->GetClientMap()->m_wMapID == m_wMapID)
			{
				m_pMyCharPos[0]->ShowComponent(TRUE);
				m_pMyCharPos[0]->MoveComponent(GetPosition(m_pHost->GetPositionX(), m_pHost->GetPositionZ(), m_pMyCharPos[0]->m_rc.Width(), m_pMyCharPos[0]->m_rc.Height()));
			}
			else
				m_pMyCharPos[0]->ShowComponent(FALSE);

			m_pMyCharPos[1]->ShowComponent(TRUE);
			m_pMyCharPos[1]->MoveComponent(GetPosition(m_ptMapIcon.x, m_ptMapIcon.z, m_pMyCharPos[1]->m_rc.Width(), m_pMyCharPos[1]->m_rc.Height()));
		}
		else
		{
			for (BYTE i = 0; i < 2; ++i)
				m_pMyCharPos[i]->ShowComponent(FALSE);

			m_pMapList->SetCurImage(-1);
		}
	}
}

DWORD CQuestNewDlg::MakeUnitID(WORD wMapID, BYTE nUnitX, BYTE nUnitY)
{
	return MAKELONG(MAKEWORD(nUnitX, nUnitY), wMapID);
}

CPoint CQuestNewDlg::GetPosition(FLOAT fPosX,
	FLOAT fPosZ,
	int nWidth,
	int nHeight)
{
	FLOAT fSCALE = ((FLOAT)m_pTMAP->m_pMAP->m_nUnitLength * m_pTMAP->m_pMAP->m_nTileLength);

	CPoint vRESULT;
	m_pMapList->GetComponentPos(&vRESULT);
	vRESULT.x += m_pMapList->m_rc.Width() / 2;
	vRESULT.y += m_pMapList->m_rc.Height() / 2;

	INT nUnitX, nUnitZ;
	GetUnitPosition(fPosX, fPosZ, nUnitX, nUnitZ);

	m_vTCENTER.x = (nUnitX * fSCALE) + (fSCALE / 2);
	m_vTCENTER.y = (nUnitZ * fSCALE) + (fSCALE / 2);

	fPosX -= m_vTCENTER.x;
	fPosZ -= m_vTCENTER.y;

	vRESULT.x += INT(fPosX * FLOAT(m_pMapList->m_rc.Width()) / fSCALE);
	vRESULT.y -= INT(fPosZ * FLOAT(m_pMapList->m_rc.Height()) / fSCALE);

	vRESULT.x -= nWidth / 2;
	vRESULT.y -= nHeight / 2;

	return vRESULT;
}

void CQuestNewDlg::OnNotify(DWORD from, WORD msg, LPVOID param)
{
	switch (msg)
	{
	case TNM_LINEUP:
		if (from == m_pLScroll->m_id)
			m_nDesiredLeftScrollY += 16;
		else
			m_nDesiredRightScrollY += 16;
		break;
	case TNM_LINEDOWN:
	{
		if (from == m_pLScroll->m_id)
			m_nDesiredLeftScrollY -= 16;
		else
			m_nDesiredRightScrollY -= 16;
	}
	break;
	case TNM_VSCROLL:
	{
		if (from == m_pLScroll->m_id)
		{
			int nRange;
			int nPos;

			m_pLScroll->GetScrollPos(nRange, nPos);
			m_nCurLeftScrollY = nRange ? -nPos : 0;
			m_nDesiredLeftScrollY = nRange ? -nPos : 0;
		}
		else
		{

			int nRange;
			int nPos;

			m_pRScroll->GetScrollPos(nRange, nPos);
			m_nCurRightScrollY = nRange ? -nPos : 0;
			m_nDesiredRightScrollY = nRange ? -nPos : 0;

			m_RightSide.ReAlign(-nPos);
		}
	}
	break;
	}

	CTClientUIBase::OnNotify(from, msg, param);
}

//CQuestList

void CQuestList::DeleteQuestList()
{
	for (auto QClass : m_mapQuest)
		for (auto Quest : QClass.second)
			Quest.DelQuest();

	for (BYTE i = 0; i < GetSize(); ++i)
		if (m_pQuestCatReg[i])
			m_pFrame->RemoveKid(m_pQuestCatReg[i]);

	m_bInit = FALSE;
	m_bCatSize = 0;

	m_mapQuest.clear();
};

void CQuestList::LoadComponent()
{
	if (!m_pFrame)
		AfxMessageBox("No Component(ASSERT)");

	for (BYTE i = 0; i < 255; ++i)
	{
		m_pQuestCatReg[i] = new TComponent(m_pFrame, m_pFrame->FindKid(26131)->m_pDESC);
		m_pQuestCatReg[i]->m_id = m_pFrame->GetUniqueID();
	}

	m_pQuestCat = new TButton(m_pFrame, static_cast<TButton*>(m_pFrame->FindKid(26130))->m_pDESC);
	m_pQuestCat->m_id = m_pFrame->GetUniqueID();
	m_pFrame->AddKid(m_pQuestCat);

	m_Opened = TRUE;
	m_bInit = FALSE;
}

void CQuestList::AddCategory(INT nID, CString strName, BYTE bCatCount)
{
	if (!m_bInit)
	{
		for (BYTE i = 0; i < GetSize(); ++i)
			m_pFrame->RemoveKid(m_pQuestCatReg[i]);

		m_mapQuest.clear();
	}

	m_pQuestCatReg[m_bCatSize]->m_strText = strName;
	m_pQuestCatReg[m_bCatSize]->m_dwMediaID = nID;
	m_pFrame->AddKid(m_pQuestCatReg[m_bCatSize]);

	auto Category = m_mapQuest.find(nID);
	if (Category == m_mapQuest.end())
		m_mapQuest.insert(std::make_pair(nID, std::vector<CQuest>{}));
	else
		Category->second.clear();

	m_bCatSize++;
	m_bInit = TRUE;
}

void CQuestList::AddQuest(DWORD dwCategoryID, CString strQuestName, CString strLevel, DWORD_PTR dwQuest, BOOL Accepted, BOOL Complete)
{
	CTClientQuest* pCQUEST = Accepted ? (CTClientQuest*)dwQuest : nullptr;
	LPTQUEST pTQUEST = nullptr;
	if (pCQUEST)
		pTQUEST = pCQUEST->m_pTQUEST;
	else
		pTQUEST = (LPTQUEST)dwQuest;

	CQuest Q;
	Q.m_pFrame = m_pFrame;
	Q.m_dwQuest = dwQuest;
	Q.m_Accepted = Accepted;
	Q.m_Completed = Complete;
	Q.AddQuest(strQuestName, strLevel);
	Q.m_pQuestName->SetTextClr(GetTQUESTColor(Q));

	auto Category = m_mapQuest.find(dwCategoryID);
	if (Category != m_mapQuest.end())
		Category->second.push_back(Q);
}

DWORD CQuestList::GetTQUESTColor(const CQuest& pQUEST) const
{
	if (pQUEST.m_Accepted || pQUEST.m_Completed)
		return 0xFFF3F3F3;

	return CQuestNewDlg::m_DefaultQColor;
}

void CQuestList::MoveCatBy(INT nY)
{
	for (auto itQuest : m_mapQuest)
	{
		for (BYTE i = 0; i < GetSize(); ++i)
		{
			if (!m_pQuestCatReg[i])
				continue;

			if (itQuest.first == m_pQuestCatReg[i]->m_dwMediaID)
				m_pQuestCatReg[i]->MoveComponentBy(0, nY);
		}

		for (BYTE i = 0; i < itQuest.second.size(); ++i)
		{
			itQuest.second[i].m_pQuestBtn->MoveComponentBy(0, nY);
			itQuest.second[i].m_pCheckBox->MoveComponentBy(0, nY);
			itQuest.second[i].m_pQuestName->MoveComponentBy(0, nY);
			itQuest.second[i].m_pProgress->MoveComponentBy(0, nY);
		}
	}

	if (m_pQuestCat)
		m_pQuestCat->MoveComponentBy(0, nY);
};

//CQuest

void CQuest::DelQuest()
{
	m_pFrame->RemoveKid(m_pCheckBox);
	m_pFrame->RemoveKid(m_pQuestName);
	m_pFrame->RemoveKid(m_pQuestBtn);
	m_pFrame->RemoveKid(m_pProgress);

	SAFE_DELETE(m_pCheckBox);
	SAFE_DELETE(m_pQuestName);
	SAFE_DELETE(m_pQuestBtn);
	SAFE_DELETE(m_pProgress);
};

void CQuest::AddQuest(CString strQName, CString strLevel)
{
	m_pCheckBox = new TButton(m_pFrame, static_cast<TButton*>(m_pFrame->FindKid(26132))->m_pSavedDESC);
	m_pQuestBtn = new TButton(m_pFrame, static_cast<TButton*>(m_pFrame->FindKid(11106))->m_pSavedDESC);
	m_pQuestName = new TComponent(m_pFrame, (m_pFrame->FindKid(26564)->m_pSavedDESC));
	m_pProgress = new TImageList(m_pFrame, static_cast<TImageList*>(m_pFrame->FindKid(26134))->m_pSavedDESC);

	m_pCheckBox->m_id = m_pFrame->GetUniqueID();
	m_pQuestBtn->m_id = m_pFrame->GetUniqueID();
	m_pQuestName->m_id = m_pFrame->GetUniqueID();
	m_pProgress->m_id = m_pFrame->GetUniqueID();

	m_pFrame->AddKid(m_pCheckBox);
	m_pFrame->AddKid(m_pQuestBtn);
	m_pFrame->AddKid(m_pQuestName);
	m_pFrame->AddKid(m_pProgress);

	m_pQuestName->m_strText.Format("Lv %s %s", strLevel, strQName);

	if (m_Accepted)
	{
		CTClientQuest* pTQUEST = (CTClientQuest*)m_dwQuest;
		if (pTQUEST)
			m_bSelected = pTQUEST->m_bCheckShowRight;
	}
	else
	{
		m_pFrame->RemoveKid(m_pProgress);
		m_pFrame->RemoveKid(m_pCheckBox);
	}
}

void CRightSide::LoadComponent()
{
	m_pRewardDot = m_pFrame->FindKid(26137);
	m_pRewardText = m_pFrame->FindKid(26138);

	m_pTermDot = m_pFrame->FindKid(26150);
	m_pTermText = m_pFrame->FindKid(26151);

	m_pSummaryDot = m_pFrame->FindKid(26147);
	m_pSummaryText = m_pFrame->FindKid(26148);
	m_pSummaryStr[0] = m_pFrame->FindKid(26149);

	m_pDialogueDot = m_pFrame->FindKid(26157);
	m_pDialogueText = m_pFrame->FindKid(26158);
	m_pDialogueStr[0] = m_pFrame->FindKid(26159);

	m_DefaultClr = m_pDialogueStr[0]->m_pFont->m_dwColor;

	m_pItemFrame[0] = m_pFrame->FindKid(9681);
	m_pItemIcon[0] = (TImageList*)m_pFrame->FindKid(8612);
	m_pTitleIcon[0] = (TImageList*)m_pFrame->FindKid(26205);
	m_pSkillIcon[0] = (TImageList*)m_pFrame->FindKid(26139);
	m_pRewardName[0] = m_pFrame->FindKid(26145);

	m_pTermStr[0] = m_pFrame->FindKid(26153);
	m_pTermRes[0] = m_pFrame->FindKid(26154);

	m_pItemFrame[0]->GetComponentPos(&m_BasePt);

	for (BYTE i = 1; i < STATIC_MAX; ++i)
	{
		m_pItemFrame[i] = new TComponent(m_pFrame, m_pItemFrame[0]->m_pDESC);
		m_pItemIcon[i] = new TImageList(m_pFrame, m_pItemIcon[0]->m_pDESC);
		m_pTitleIcon[i] = new TImageList(m_pFrame, m_pTitleIcon[0]->m_pDESC);
		m_pSkillIcon[i] = new TImageList(m_pFrame, m_pSkillIcon[0]->m_pDESC);
		m_pRewardName[i] = new TComponent(m_pFrame, m_pRewardName[0]->m_pDESC);
		m_pTermStr[i] = new TComponent(m_pFrame, m_pTermStr[0]->m_pDESC);
		m_pTermRes[i] = new TComponent(m_pFrame, m_pTermRes[0]->m_pDESC);
		m_pSummaryStr[i] = new TComponent(m_pFrame, m_pSummaryStr[0]->m_pDESC);
		m_pDialogueStr[i] = new TComponent(m_pFrame, m_pDialogueStr[0]->m_pDESC);

		m_pItemFrame[i]->m_id = m_pFrame->GetUniqueID();
		m_pItemIcon[i]->m_id = m_pFrame->GetUniqueID();
		m_pTitleIcon[i]->m_id = m_pFrame->GetUniqueID();
		m_pSkillIcon[i]->m_id = m_pFrame->GetUniqueID();
		m_pRewardName[i]->m_id = m_pFrame->GetUniqueID();
		m_pTermStr[i]->m_id = m_pFrame->GetUniqueID();
		m_pTermRes[i]->m_id = m_pFrame->GetUniqueID();
		m_pSummaryStr[i]->m_id = m_pFrame->GetUniqueID();
		m_pDialogueStr[i]->m_id = m_pFrame->GetUniqueID();

		m_pFrame->AddKid(m_pItemFrame[i]);
		m_pFrame->AddKid(m_pItemIcon[i]);
		m_pFrame->AddKid(m_pTitleIcon[i]);
		m_pFrame->AddKid(m_pSkillIcon[i]);
		m_pFrame->AddKid(m_pRewardName[i]);
		m_pFrame->AddKid(m_pTermStr[i]);
		m_pFrame->AddKid(m_pTermRes[i]);
		m_pFrame->AddKid(m_pSummaryStr[i]);
		m_pFrame->AddKid(m_pDialogueStr[i]);
	}

	m_pExpBox[0] = m_pFrame->FindKid(26156);
	m_pExpBox[1] = m_pFrame->FindKid(11801);
	m_pExpVal = m_pFrame->FindKid(11908);

	m_pSoulBox[0] = m_pFrame->FindKid(26539);
	m_pSoulBox[1] = m_pFrame->FindKid(26540);
	m_pSoulVal = m_pFrame->FindKid(26541);

	m_pAVBox[0] = m_pFrame->FindKid(27994);
	m_pAVBox[1] = m_pFrame->FindKid(27996);
	m_pAVVal = m_pFrame->FindKid(27995);

	m_pMoneyBox[0] = m_pFrame->FindKid(26155);
	m_pMoneyBox[1] = m_pFrame->FindKid(16461);

	for (BYTE i = 0; i < 3; ++i)
		m_pMoney[i] = m_pFrame->FindKid(2570 + i);
}

void CRightSide::Reset()
{
	m_RewardCnt = 0;
	m_TermCnt = 0;
	m_SummaryLnCnt = 0;
	m_DialogueLnCnt = 0;

	m_Reward = FALSE;
	m_Goal = FALSE;
	m_Summary = FALSE;
	m_Dialogue = FALSE;

	m_Index = 0;

	m_pRewardDot->MoveComponent(CPoint(m_pRewardDot->m_rc.left, m_BasePt.y - 24));
	m_pRewardText->MoveComponent(CPoint(m_pRewardText->m_rc.left, m_BasePt.y - 24));

	m_pTermDot->MoveComponent(CPoint(m_pTermDot->m_rc.left, m_BasePt.y));
	m_pTermText->MoveComponent(CPoint(m_pTermText->m_rc.left, m_BasePt.y));

	m_pSummaryDot->MoveComponent(CPoint(m_pSummaryDot->m_rc.left, m_BasePt.y));
	m_pSummaryText->MoveComponent(CPoint(m_pSummaryText->m_rc.left, m_BasePt.y));

	m_pDialogueDot->MoveComponent(CPoint(m_pDialogueDot->m_rc.left, m_BasePt.y));
	m_pDialogueText->MoveComponent(CPoint(m_pDialogueText->m_rc.left, m_BasePt.y));

	for (BYTE i = 0; i < STATIC_MAX; ++i)
	{
		m_pFrame->RemoveKid(m_pItemFrame[i]);
		m_pFrame->RemoveKid(m_pItemIcon[i]);
		m_pFrame->RemoveKid(m_pTitleIcon[i]);
		m_pFrame->RemoveKid(m_pSkillIcon[i]);
		m_pFrame->RemoveKid(m_pRewardName[i]);
		m_pFrame->RemoveKid(m_pTermStr[i]);
		m_pFrame->RemoveKid(m_pTermRes[i]);
		m_pFrame->RemoveKid(m_pSummaryStr[i]);
		m_pFrame->RemoveKid(m_pDialogueStr[i]);

		m_pItemFrame[i]->MoveComponent(m_BasePt);
		m_pItemIcon[i]->MoveComponent(CPoint(m_pItemIcon[0]->m_rc.left, m_BasePt.y + 4));
		m_pTitleIcon[i]->MoveComponent(CPoint(m_pTitleIcon[0]->m_rc.left, m_BasePt.y + 4));
		m_pSkillIcon[i]->MoveComponent(CPoint(m_pSkillIcon[0]->m_rc.left, m_BasePt.y + 4));
		m_pRewardName[i]->MoveComponent(CPoint(m_pRewardName[0]->m_rc.left, m_BasePt.y + 4));
		m_pTermStr[i]->MoveComponent(CPoint(m_pTermStr[0]->m_rc.left, m_BasePt.y));
		m_pTermRes[i]->MoveComponent(CPoint(m_pTermRes[0]->m_rc.left, m_BasePt.y));
		m_pSummaryStr[i]->MoveComponent(CPoint(m_pSummaryStr[0]->m_rc.left, m_BasePt.y));
		m_pDialogueStr[i]->MoveComponent(CPoint(m_pDialogueStr[0]->m_rc.left, m_BasePt.y));
	}

	for (BYTE i = 0; i < BOX_PH_COUNT; ++i)
	{
		m_pFrame->RemoveKid(m_pExpBox[i]);
		m_pExpBox[i]->MoveComponent(CPoint(m_pExpBox[i]->m_rc.left, m_BasePt.y));

		m_pFrame->RemoveKid(m_pSoulBox[i]);
		m_pSoulBox[i]->MoveComponent(CPoint(m_pSoulBox[i]->m_rc.left, m_BasePt.y));

		m_pFrame->RemoveKid(m_pAVBox[i]);
		m_pAVBox[i]->MoveComponent(CPoint(m_pAVBox[i]->m_rc.left, m_BasePt.y));
	}

	m_pFrame->RemoveKid(m_pExpVal);
	m_pExpVal->MoveComponent(CPoint(m_pExpVal->m_rc.left, m_BasePt.y));

	m_pFrame->RemoveKid(m_pSoulVal);
	m_pSoulVal->MoveComponent(CPoint(m_pExpVal->m_rc.left, m_BasePt.y));

	m_pFrame->RemoveKid(m_pAVVal);
	m_pAVVal->MoveComponent(CPoint(m_pAVVal->m_rc.left, m_BasePt.y));

	for (BYTE i = 0; i < 3; ++i)
	{
		if (i < 2)
		{
			m_pFrame->RemoveKid(m_pMoneyBox[i]);
			m_pMoneyBox[i]->MoveComponent(CPoint(m_pMoneyBox[i]->m_rc.left, m_BasePt.y));
		}

		m_pFrame->RemoveKid(m_pMoney[i]);
		m_pMoney[i]->MoveComponent(CPoint(m_pMoney[i]->m_rc.left, m_BasePt.y));
	}

	m_pRewardDot->ShowComponent(FALSE);
	m_pRewardText->ShowComponent(FALSE);

	m_pTermDot->ShowComponent(FALSE);
	m_pTermText->ShowComponent(FALSE);

	m_pSummaryDot->ShowComponent(FALSE);
	m_pSummaryText->ShowComponent(FALSE);
}

void CRightSide::ResetPos()
{
	m_pRewardDot->MoveComponent(CPoint(m_pRewardDot->m_rc.left, m_BasePt.y - 24));
	m_pRewardText->MoveComponent(CPoint(m_pRewardText->m_rc.left, m_BasePt.y - 24));

	m_pTermDot->MoveComponent(CPoint(m_pTermDot->m_rc.left, m_BasePt.y));
	m_pTermText->MoveComponent(CPoint(m_pTermText->m_rc.left, m_BasePt.y));

	m_pSummaryDot->MoveComponent(CPoint(m_pSummaryDot->m_rc.left, m_BasePt.y));
	m_pSummaryText->MoveComponent(CPoint(m_pSummaryText->m_rc.left, m_BasePt.y));

	m_pDialogueDot->MoveComponent(CPoint(m_pDialogueDot->m_rc.left, m_BasePt.y));
	m_pDialogueText->MoveComponent(CPoint(m_pDialogueText->m_rc.left, m_BasePt.y));

	for (BYTE i = 0; i < STATIC_MAX; ++i)
	{
		m_pItemFrame[i]->MoveComponent(m_BasePt);
		m_pItemIcon[i]->MoveComponent(CPoint(m_pItemIcon[0]->m_rc.left, m_BasePt.y + 4));
		m_pTitleIcon[i]->MoveComponent(CPoint(m_pTitleIcon[0]->m_rc.left, m_BasePt.y + 4));
		m_pSkillIcon[i]->MoveComponent(CPoint(m_pSkillIcon[0]->m_rc.left, m_BasePt.y + 4));
		m_pRewardName[i]->MoveComponent(CPoint(m_pRewardName[0]->m_rc.left, m_BasePt.y + 4));
		m_pTermStr[i]->MoveComponent(CPoint(m_pTermStr[0]->m_rc.left, m_BasePt.y));
		m_pTermRes[i]->MoveComponent(CPoint(m_pTermRes[0]->m_rc.left, m_BasePt.y));
		m_pSummaryStr[i]->MoveComponent(CPoint(m_pSummaryStr[0]->m_rc.left, m_BasePt.y));
		m_pDialogueStr[i]->MoveComponent(CPoint(m_pDialogueStr[0]->m_rc.left, m_BasePt.y));
	}

	for (BYTE i = 0; i < BOX_PH_COUNT; ++i)
	{
		m_pExpBox[i]->MoveComponent(CPoint(m_pExpBox[i]->m_rc.left, m_BasePt.y));
		m_pSoulBox[i]->MoveComponent(CPoint(m_pSoulBox[i]->m_rc.left, m_BasePt.y));
		m_pAVBox[i]->MoveComponent(CPoint(m_pAVBox[i]->m_rc.left, m_BasePt.y));
	}

	m_pExpVal->MoveComponent(CPoint(m_pExpVal->m_rc.left, m_BasePt.y));
	m_pSoulVal->MoveComponent(CPoint(m_pExpVal->m_rc.left, m_BasePt.y));
	m_pAVVal->MoveComponent(CPoint(m_pAVVal->m_rc.left, m_BasePt.y));

	for (BYTE i = 0; i < 3; ++i)
	{
		if (i < 2)
			m_pMoneyBox[i]->MoveComponent(CPoint(m_pMoneyBox[i]->m_rc.left, m_BasePt.y));

		m_pMoney[i]->MoveComponent(CPoint(m_pMoney[i]->m_rc.left, m_BasePt.y));
	}
}

void CRightSide::AddReward(BYTE Type, DWORD Value, BYTE Count, const LPTREWARD Reward, DWORD dwQuestID)
{
	m_pRewardDot->ShowComponent(TRUE);
	m_pRewardText->ShowComponent(TRUE);
	m_Reward = TRUE;

	if (Type != RT_EXP && Type != RT_GOLD && Type != RT_SOUL && Type != RT_POINT)
		m_pFrame->AddKid(m_pItemFrame[m_RewardCnt]);

	switch (Type)
	{
	case RT_ITEM:
	case RT_MAGICITEM:
	{
		if (Type == RT_ITEM)
		{
			LPTITEM pITEM = CTChart::FindTITEMTEMP((WORD)Value);
			if (pITEM)
			{
				LPTITEMVISUAL pVISUAL = CTChart::FindTITEMVISUAL(pITEM->m_wVisual[0]);
				if (pVISUAL)
					m_pItemIcon[m_RewardCnt]->SetCurImage(pVISUAL->m_wIcon);
				m_pRewardName[m_RewardCnt]->m_strText = pITEM->m_strNAME;
				if (Count > 1) {
					CString strAdd;
					strAdd.Format("\n%d", Count);
					m_pRewardName[m_RewardCnt]->m_strText += strAdd;
				}
			}
		}
		else
		{
			LPTQUESTITEM pQuestItem = CTChart::FindTQUESTMAGICITEM((WORD)Value);
			if (pQuestItem)
			{
				LPTITEM pITEM = CTChart::FindTITEMTEMP(pQuestItem->m_wItemID);
				if (pITEM)
				{
					LPTITEMVISUAL pVISUAL = CTChart::FindTITEMVISUAL(pITEM->m_wVisual[0]);
					if (pVISUAL)
						m_pItemIcon[m_RewardCnt]->SetCurImage(pVISUAL->m_wIcon);
					m_pRewardName[m_RewardCnt]->m_strText = pITEM->m_strNAME;

					if (Count > 1) {
						CString strAdd;
						strAdd.Format("\n%d", Count);
						m_pRewardName[m_RewardCnt]->m_strText += strAdd;
					}
				}
			}
		}

		m_pFrame->AddKid(m_pItemIcon[m_RewardCnt]);
		m_pFrame->AddKid(m_pRewardName[m_RewardCnt]);
	}
	break;
	case RT_SKILL:
	case RT_SKILLUP:
	{
		LPTSKILL pSKILL = CTChart::FindTSKILLTEMP((WORD)Value);
		if (pSKILL) {
			m_pSkillIcon[m_RewardCnt]->SetCurImage(pSKILL->m_wIconID);
			m_pRewardName[m_RewardCnt]->m_strText = pSKILL->m_strNAME;
		}

		m_pFrame->AddKid(m_pSkillIcon[m_RewardCnt]);
		m_pFrame->AddKid(m_pRewardName[m_RewardCnt]);
	}
	break;
	case RT_TITLE:
	{
		m_pTitleIcon[m_RewardCnt]->SetCurImage(0);

		LPTTITLE pTITLE = nullptr;
		for (const auto& Title : CTChart::m_mapTTITLETEMP)
		{
			if (Title.second->m_bKind == QUEST_TITLE && Title.second->m_dwRequirement == dwQuestID)
				pTITLE = Title.second;
		}

		if (pTITLE)
			m_pRewardName[m_RewardCnt]->m_strText.Format("Title : %s", pTITLE->m_strTitle);

		m_pFrame->AddKid(m_pTitleIcon[m_RewardCnt]);
		m_pFrame->AddKid(m_pRewardName[m_RewardCnt]);
	}
	break;
	case RT_EXP:
	{
		m_pExpVal->m_strText.Format("%d", Value);

		for (BYTE i = 0; i < BOX_PH_COUNT; ++i) {
			m_pFrame->AddKid(m_pExpBox[i]);
			m_pExpBox[i]->MoveComponentBy(0, m_Index);
		}

		m_pFrame->AddKid(m_pExpVal);
	}
	break;
	case RT_GOLD:
	{
		DWORD dwMONEY[3] = { 0, 0, 0 };

		CTClientGame::SplitMoney(
			Value,
			&dwMONEY[0],
			&dwMONEY[1],
			&dwMONEY[2]);

		for (INT i = 0; i < 3; ++i)
		{
			if (i < 2)
				m_pFrame->AddKid(m_pMoneyBox[i]);

			m_pMoney[i]->m_strText.Format("%d", dwMONEY[i]);
			m_pFrame->AddKid(m_pMoney[i]);
		}
	}
	case RT_SOUL:
	{
		m_pSoulVal->m_strText.Format("%d", Value);

		for (BYTE i = 0; i < BOX_PH_COUNT; ++i)
			m_pFrame->AddKid(m_pSoulBox[i]);

		m_pFrame->AddKid(m_pSoulVal);
	}
	break;
	case RT_POINT:
	{
		m_pAVVal->m_strText.Format("%d", Value);

		for (BYTE i = 0; i < BOX_PH_COUNT; ++i)
			m_pFrame->AddKid(m_pAVBox[i]);

		m_pFrame->AddKid(m_pAVVal);
	}
	break;
	}

	if (Type != RT_EXP && Type != RT_GOLD && Type != RT_SOUL && Type != RT_POINT)
	{
		m_pRewardName[m_RewardCnt]->SaveParam((DWORD_PTR)Reward);
		m_RewardCnt++;
	}
}

void CRightSide::ReAlign(INT ScrollY)
{
	ResetPos();
	m_Index = ScrollY;

	if (!m_Reward)
		m_Index -= 24;

	m_pRewardDot->MoveComponentBy(0, m_Index);
	m_pRewardText->MoveComponentBy(0, m_Index);

	for (BYTE i = 0; i < m_RewardCnt; ++i)
	{
		if (m_pFrame->FindKid(m_pRewardName[i]->m_id))
		{
			if (m_pFrame->FindKid(m_pItemFrame[i]->m_id))
				m_pItemFrame[i]->MoveComponentBy(0, m_Index);

			if (m_pFrame->FindKid(m_pItemIcon[i]->m_id))
				m_pItemIcon[i]->MoveComponentBy(0, m_Index);

			if (m_pFrame->FindKid(m_pSkillIcon[i]->m_id))
				m_pSkillIcon[i]->MoveComponentBy(0, m_Index);

			if (m_pFrame->FindKid(m_pTitleIcon[i]->m_id))
				m_pTitleIcon[i]->MoveComponentBy(0, m_Index);

			if (m_pFrame->FindKid(m_pRewardName[i]->m_id))
				m_pRewardName[i]->MoveComponentBy(0, m_Index);

			m_Index += 48;
		}
	}

	for (BYTE i = 0; i < BOX_PH_COUNT; ++i)
		m_pExpBox[i]->MoveComponentBy(0, m_Index);
	m_pExpVal->MoveComponentBy(0, m_Index);

	if (m_pFrame->FindKid(m_pExpVal->m_id))
		m_Index += 24;

	for (BYTE i = 0; i < 3; ++i)
	{
		if (i < 2)
			m_pMoneyBox[i]->MoveComponentBy(0, m_Index - 24);

		m_pMoney[i]->MoveComponentBy(0, m_Index - 24);
	}

	for (BYTE i = 0; i < BOX_PH_COUNT; ++i)
		m_pSoulBox[i]->MoveComponentBy(0, m_Index);
	m_pSoulVal->MoveComponentBy(0, m_Index);

	if (m_pFrame->FindKid(m_pSoulVal->m_id))
		m_Index += 24;

	for (BYTE i = 0; i < BOX_PH_COUNT; ++i)
		m_pAVBox[i]->MoveComponentBy(0, m_Index);
	m_pAVVal->MoveComponentBy(0, m_Index);

	if (m_pFrame->FindKid(m_pAVVal->m_id))
		m_Index += 24;

	m_pTermDot->MoveComponentBy(0, m_Index);
	m_pTermText->MoveComponentBy(0, m_Index);

	if (m_Goal)
		m_Index += 24;

	for (BYTE i = 0; i < m_TermCnt; ++i)
	{
		m_pTermStr[i]->MoveComponentBy(0, m_Index);
		m_pTermRes[i]->MoveComponentBy(0, m_Index);

		m_Index += 24;
	}

	m_pSummaryDot->MoveComponentBy(0, m_Index);
	m_pSummaryText->MoveComponentBy(0, m_Index);

	if (m_Summary)
		m_Index += 24;

	for (BYTE i = 0; i < m_SummaryLnCnt; ++i) {
		m_pSummaryStr[i]->MoveComponentBy(0, m_Index);
		m_Index += 24;
	}

	m_pDialogueDot->MoveComponentBy(0, m_Index);
	m_pDialogueText->MoveComponentBy(0, m_Index);

	if (m_Dialogue)
		m_Index += 24;

	for (BYTE i = 0; i < m_DialogueLnCnt; ++i) {
		m_pDialogueStr[i]->MoveComponentBy(0, m_Index);
		m_Index += 24;
	}
}

void CRightSide::AddTerm(const LPTTERM& Term, BYTE bCount)
{
	m_pTermDot->ShowComponent(TRUE);
	m_pTermText->ShowComponent(TRUE);
	m_Goal = TRUE;

	m_pTermStr[m_TermCnt]->m_strText.Format("%d.%s", m_TermCnt + 1, Term->m_strTermMSG);

	if (bCount >= Term->m_bCount)
		m_pTermRes[m_TermCnt]->m_strText = "(finished)";
	else
		m_pTermRes[m_TermCnt]->m_strText.Format("(%d/%d)", bCount, Term->m_bCount);

	m_pFrame->AddKid(m_pTermStr[m_TermCnt]);
	m_pFrame->AddKid(m_pTermRes[m_TermCnt]);

	m_TermCnt++;
}

void CRightSide::SummaryMsg(const CString& strSummary)
{
	m_pSummaryDot->ShowComponent(TRUE);
	m_pSummaryText->ShowComponent(TRUE);
	m_Summary = TRUE;

	CSize szITEM = CSize(350, 20);
	CSize szTEXT(0, 0);

	CString strFORMAT;
	CString strMSG;

	int nPOS = 0;

	CSize szSpaceSize;
	m_pSummaryStr[0]->GetTextExtentPoint(" ", szSpaceSize);

	CString strLINE;
	CSize szLINE(0, 0);

	strMSG = strSummary.Tokenize(" \n", nPOS);
	while (!strMSG.IsEmpty())
	{
		strMSG.Remove('\r');
		strMSG.Replace("%s", CTClientGame::GetInstance()->GetMainChar()->GetName());

		CSize szMSG;
		m_pSummaryStr[0]->GetTextExtentPoint(LPCTSTR(strMSG), szMSG);

		if (szMSG.cx + szLINE.cx <= szITEM.cx)
		{
			strLINE += strMSG;
			szLINE.cx += szMSG.cx;
		}
		else
		{
			m_pSummaryStr[m_SummaryLnCnt]->m_strText = strLINE;
			m_pFrame->AddKid(m_pSummaryStr[m_SummaryLnCnt]);
			m_SummaryLnCnt++;

			szLINE.cx = szMSG.cx;
			strLINE = strMSG;
		}

		BYTE c = strSummary.GetAt(nPOS - 1);
		if (c == ' ')
		{
			strLINE += ' ';
			szLINE.cx += szSpaceSize.cx;
		}
		else if (c == '\n')
		{
			m_pSummaryStr[m_SummaryLnCnt]->m_strText = strLINE;
			m_pFrame->AddKid(m_pSummaryStr[m_SummaryLnCnt]);
			m_SummaryLnCnt++;

			strLINE.Empty();
			szLINE.cx = 0;
		}

		strMSG = strSummary.Tokenize(" \n", nPOS);
	}

	if (!strLINE.IsEmpty())
	{
		m_pSummaryStr[m_SummaryLnCnt]->m_strText = strLINE;
		m_pFrame->AddKid(m_pSummaryStr[m_SummaryLnCnt]);
		m_SummaryLnCnt++;
	}
}

void CRightSide::DialogueMsg(CString& strTitle, const CString& strDialogue)
{
	m_pDialogueDot->ShowComponent(TRUE);
	m_pDialogueText->ShowComponent(TRUE);
	m_Dialogue = TRUE;

	CSize szITEM = CSize(350, 20);
	CSize szTEXT(0, 0);

	CString strFORMAT;
	CString strMSG;

	int nPOS = 0;

	if (!strTitle.IsEmpty())
	{
		strTitle.Replace(_T("%s"), CTClientGame::GetInstance()->GetMainChar()->GetName());
		m_pDialogueStr[m_DialogueLnCnt]->m_strText = strTitle;
		m_pDialogueStr[m_DialogueLnCnt]->SetTextClr(m_pAVVal->m_pFont->m_dwColor);
		m_pFrame->AddKid(m_pDialogueStr[m_DialogueLnCnt]);
		m_DialogueLnCnt++;
	}

	CSize szSpaceSize(0, 0);
	m_pDialogueStr[0]->GetTextExtentPoint(" ", szSpaceSize);

	CString strLINE;
	CSize szLINE(0, 0);

	strMSG = strDialogue.Tokenize(" \n", nPOS);
	while (!strMSG.IsEmpty())
	{
		strMSG.Remove('\r');
		strMSG.Replace("%s", CTClientGame::GetInstance()->GetMainChar()->GetName());

		CSize szMSG;
		m_pDialogueStr[0]->GetTextExtentPoint(LPCTSTR(strMSG), szMSG);

		if (szMSG.cx + szLINE.cx <= szITEM.cx)
		{
			strLINE += strMSG;
			szLINE.cx += szMSG.cx;
		}
		else
		{
			strTitle.Replace(_T("%s"), CTClientGame::GetInstance()->GetMainChar()->GetName());
			m_pDialogueStr[m_DialogueLnCnt]->m_strText = strLINE;
			m_pDialogueStr[m_DialogueLnCnt]->SetTextClr(m_DefaultClr);
			m_pFrame->AddKid(m_pDialogueStr[m_DialogueLnCnt]);
			m_DialogueLnCnt++;

			szLINE.cx = szMSG.cx;
			strLINE = strMSG;
		}

		BYTE c = strDialogue.GetAt(nPOS - 1);
		if (c == ' ')
		{
			strLINE += ' ';
			szLINE.cx += szSpaceSize.cx;
		}
		else if (c == '\n')
		{
			strTitle.Replace(_T("%s"), CTClientGame::GetInstance()->GetMainChar()->GetName());
			m_pDialogueStr[m_DialogueLnCnt]->m_strText = strLINE;
			m_pDialogueStr[m_DialogueLnCnt]->SetTextClr(m_DefaultClr);
			m_pFrame->AddKid(m_pDialogueStr[m_DialogueLnCnt]);
			m_DialogueLnCnt++;

			strLINE.Empty();
			szLINE.cx = 0;
		}

		strMSG = strDialogue.Tokenize(" \n", nPOS);
	}

	if (!strLINE.IsEmpty())
	{
		strTitle.Replace(_T("%s"), CTClientGame::GetInstance()->GetMainChar()->GetName());
		m_pDialogueStr[m_DialogueLnCnt]->m_strText = strLINE;
		m_pDialogueStr[m_DialogueLnCnt]->SetTextClr(m_DefaultClr);
		m_pFrame->AddKid(m_pDialogueStr[m_DialogueLnCnt]);
		m_DialogueLnCnt++;
	}
}