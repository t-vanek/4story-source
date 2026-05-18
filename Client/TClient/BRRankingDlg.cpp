#include "StdAfx.h"
#include "BRRankingDlg.h"
#include "BRTRanking.h"
#include "TClientGame.h"
//#include "TBRRankClass.h"
#include "TClientWnd.h"

CBRRankingDlg::CBRRankingDlg(TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc)
: CTClientUIBase(pParent, pDesc)
{
	CPoint pRanking, pName, pServer, pBP, pBB;

	m_pNRanking[0] = FindKid(28224);
	m_pName[0] = FindKid(28340);
	m_pServer[0] = FindKid(28398);
	m_pKills[0] = FindKid(28250);
	m_pLifes[0] = FindKid(28264);

	m_pNRanking[0]->GetComponentPos(&pRanking);
	m_pName[0]->GetComponentPos(&pName);
	m_pServer[0]->GetComponentPos(&pServer);
	m_pKills[0]->GetComponentPos(&pBP);
	m_pLifes[0]->GetComponentPos(&pBB);

	for(BYTE i = 1 ; i < BR_RANKING_UI_SIZE; ++i)
	{
		m_pNRanking[0]->GetComponentPos(&pRanking);
		m_pName[0]->GetComponentPos(&pName);
		m_pServer[0]->GetComponentPos(&pServer);
		m_pKills[0]->GetComponentPos(&pBP);
		m_pLifes[0]->GetComponentPos(&pBB);

		pRanking.y += i * 27;
		pName.y += i * 27;
		pServer.y += i * 27;
		pBP.y += i * 27;
		pBB.y += i * 27;

		m_pNRanking[i] = new TComponent(this, m_pNRanking[0]->m_pDESC);
		m_pName[i] = new TComponent(this, m_pName[0]->m_pDESC);
		m_pServer[i] = new TComponent(this, m_pServer[0]->m_pDESC);
		m_pKills[i] = new TComponent(this, m_pKills[0]->m_pDESC);
		m_pLifes[i] = new TComponent(this, m_pLifes[0]->m_pDESC);

		m_pNRanking[i]->m_id = GetUniqueID();
		m_pName[i]->m_id = GetUniqueID();
		m_pServer[i]->m_id = GetUniqueID();
		m_pKills[i]->m_id = GetUniqueID();
		m_pLifes[i]->m_id = GetUniqueID();

		m_pNRanking[i]->MoveComponent(pRanking);
		m_pName[i]->MoveComponent(pName);
		m_pServer[i]->MoveComponent(pServer);
		m_pKills[i]->MoveComponent(pBP);
		m_pLifes[i]->MoveComponent(pBB);

		AddKid(m_pNRanking[i]);
		AddKid(m_pName[i]);
		AddKid(m_pServer[i]);
		AddKid(m_pKills[i]);
		AddKid(m_pLifes[i]);
	}
/*
	for (TCOMP_LIST::iterator it = m_kids.begin(); it != m_kids.end(); ++it)
	{
		if ((*it)->m_bType == TCML_TYPE_IMAGELIST)
		{
			RemoveKid(*it);
			delete (*it);
		}
	}
*/

	DWORD bStyle = GetStyle();
	SetStyle(bStyle |+ TS_CUSTOM_COLOR);
	m_dwColor = 0xCBFFFFFF;

	m_vBRRANK.clear();
}

CBRRankingDlg::~CBRRankingDlg()
{
	ReleaseRanking();
}

BOOL Sorter(const LPBRTPLAYER& First, const LPBRTPLAYER& Second)
{
	if(First->m_wKills < Second->m_wKills)
		return FALSE;
	else if(First->m_wKills == Second->m_wKills)
	{
		if(First->m_bLifes < Second->m_bLifes)
			return FALSE;
		else
			return TRUE;
	}

	return TRUE;
}

void CBRRankingDlg::ReleaseRanking()
{
	m_vBRRANK.clear();
}

void CBRRankingDlg::CloneTRanking(LPBRTPLAYER Player)
{
	m_vBRRANK.push_back(Player);
}

void CBRRankingDlg::UpdateRanking(DWORD dwCharID, WORD wKills, BYTE bLifes)
{
	for (std::vector<LPBRTPLAYER>::iterator it = m_vBRRANK.begin(); it != m_vBRRANK.end(); ++it)
	{
		if ((*it)->m_dwCharID == dwCharID)
		{
			(*it)->m_wKills = wKills;
			(*it)->m_bLifes = bLifes;

			break;
		}
	}
}

void CBRRankingDlg::ResetRank()
{
	sort(m_vBRRANK.begin(), m_vBRRANK.end(), Sorter);
	for (BYTE i = 0; i < BR_RANKING_UI_SIZE; ++i)
	{
		m_pNRanking[i]->m_strText.Empty();
		m_pName[i]->m_strText.Empty();
		m_pServer[i]->m_strText.Empty();
		m_pKills[i]->m_strText.Empty();
		m_pLifes[i]->m_strText.Empty();
	}

	for (size_t i = 0; i < m_vBRRANK.size(); ++i)
	{
		if (m_vBRRANK[i]->m_dwCharID == CTClientGame::GetInstance()->GetMainChar()->m_dwID)
		{
			m_pNRanking[0]->m_strText.Format("%d", i + 1);
			m_pName[0]->m_strText = m_vBRRANK[i]->m_strNAME;
			m_pServer[0]->m_strText = "4Story";
			m_pKills[0]->m_strText.Format("%d", m_vBRRANK[i]->m_wKills);
			m_pLifes[0]->m_strText.Format("%d", m_vBRRANK[i]->m_bLifes);
			break;
		}
	}

//	if (m_vBRRANK.size() > BR_RANKING_UI_SIZE - 1)
//		m_vBRRANK.resize(BR_RANKING_UI_SIZE - 1);

	for (int i = 0; i < BR_RANKING_UI_SIZE - 1; ++i)
	{
		if (i >= m_vBRRANK.size())
			break;

		if (!m_vBRRANK[i]->m_wKills)
			continue;

		m_pNRanking[1 + i]->m_strText.Format("%d", 1 + i);
		m_pName[1 + i]->m_strText = m_vBRRANK[i]->m_strNAME;
		m_pServer[1 + i]->m_strText = "4Story";
		m_pKills[1 + i]->m_strText.Format("%d", m_vBRRANK[i]->m_wKills);
		m_pLifes[1 + i]->m_strText.Format("%d", m_vBRRANK[i]->m_bLifes);
	}

	UseOwnImages(25005);
	MoveComponent(CPoint(
		CTClientUIBase::m_vBasis[TBASISPOINT_CENTER_MIDDLE].x - 425,
		CTClientUIBase::m_vBasis[TBASISPOINT_CENTER_MIDDLE].y - 340));
	ShowComponent(TRUE);
}