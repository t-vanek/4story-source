#include "stdafx.h"
#include "BRTRanking.h"
#include "TClientGame.h"
#include "TCustomStrings.h"
#include "BRRankingDlg.h"

CBRTRanking::CBRTRanking(TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc)
: CTClientUIBase(pParent, pDesc)
{
	static const DWORD dwStartName = 26487;
	static const DWORD dwStartKills = 26349;
	static const DWORD dwStartButtons = 26515;

	for(DWORD i = dwStartName; i < dwStartName + BR_RANK_COUNT; ++i)
		m_pName[i - dwStartName] = FindKid(i);

	for(DWORD i = dwStartKills; i < dwStartKills + BR_RANK_COUNT; ++i)
		m_pKills[i - dwStartKills] = FindKid(i);

	for(DWORD i = dwStartButtons; i < dwStartButtons + BR_RANK_COUNT; ++i)
		m_pButtons[i - dwStartButtons] = (TButton*) FindKid(i);

	m_vBRTRanking.clear();
}

BOOL CBRTRanking::CanWithItemUI()
{
	return TRUE;
}

CBRTRanking::~CBRTRanking()
{
	ReleaseTeams();
}

void CBRTRanking::ReleaseTeams()
{
	while (!m_vBRTRanking.empty())
	{
		if (m_vBRTRanking.back())
		{
			for (MAPBRTPLAYER::iterator itTPlayer = m_vBRTRanking.back()->m_mapBRTPlayers.begin(); itTPlayer != m_vBRTRanking.back()->m_mapBRTPlayers.end(); ++itTPlayer)
			{
				if ((*itTPlayer).second) //Check because it is also being deleted at the ranking
					delete (*itTPlayer).second;
			}
			m_vBRTRanking.back()->m_mapBRTPlayers.clear();

			delete m_vBRTRanking.back();
		}

		m_vBRTRanking.pop_back();
	}

	static_cast<CBRRankingDlg*>(CTClientGame::GetInstance()->GetFrame(TFRAME_BRRANKING))->ReleaseRanking();
}

LPBRTRANKING CBRTRanking::FindTeam(DWORD dwTeamID)
{
	for (VECTORBRTRANKING::iterator it = m_vBRTRanking.begin(); it != m_vBRTRanking.end(); ++it)
	{
		if ((*it)->m_dwTeamID == dwTeamID)
			return (*it);
	}

	return NULL;
}

void CBRTRanking::AddTeam(DWORD dwTeamID, const MAPBRTPLAYER& mapTeamPlayer)
{
	if (FindTeam(dwTeamID))
		return;
	
	LPBRTRANKING pTEAM = new BRTRANKING();
	pTEAM->m_dwTeamID = dwTeamID;
	pTEAM->m_mapBRTPlayers = mapTeamPlayer; //we are passing the pointers so we can clear them later with ReleaseTeam and no memory will be leaked.
	m_vBRTRanking.push_back(pTEAM);
	
	RecalcTotalKills(dwTeamID);
	UpdateUI();
}

WORD CBRTRanking::RecalcTotalKills(DWORD dwTeamID)
{
	LPBRTRANKING pTEAM = FindTeam(dwTeamID);
	if (pTEAM)
	{
		pTEAM->m_wTotalKills = 0;
		for (MAPBRTPLAYER::iterator it = pTEAM->m_mapBRTPlayers.begin(); it != pTEAM->m_mapBRTPlayers.end(); ++it)
			pTEAM->m_wTotalKills += (*it).second->m_wKills;

		return pTEAM->m_wTotalKills;
	}

	return 0;
}

void CBRTRanking::UpdatePlayer(DWORD dwTeamID, DWORD dwCharID, WORD wKills)
{
	LPBRTRANKING pTEAM = FindTeam(dwTeamID);
	if (pTEAM)
	{
		MAPBRTPLAYER::iterator finder = pTEAM->m_mapBRTPlayers.find(dwCharID);
		if (finder != pTEAM->m_mapBRTPlayers.end())
			(*finder).second->m_wKills = wKills;

		RecalcTotalKills(dwTeamID);
	}

	UpdateUI();
}

BOOL SortRanks( const LPBRTRANKING& First, const LPBRTRANKING& Second )
{
	if(First->m_wTotalKills > Second->m_wTotalKills)
		return TRUE;

	return FALSE;
}

void CBRTRanking::UpdateUI()
{
	std::sort(m_vBRTRanking.begin(), m_vBRTRanking.end(), SortRanks);

	for(BYTE i = 0; i < BR_RANK_COUNT; ++i)
	{
		if (i < m_vBRTRanking.size())
		{
			LPBRTRANKING pTEAM = m_vBRTRanking.at(i);

			if (pTEAM && pTEAM->m_wTotalKills)
			{
				MAPBRTPLAYER::iterator itBegin = pTEAM->m_mapBRTPlayers.find(CTClientGame::GetInstance()->GetMainChar()->m_dwID);//.begin();
				if (itBegin == pTEAM->m_mapBRTPlayers.end())
				{
					itBegin = pTEAM->m_mapBRTPlayers.begin();
					if (itBegin == pTEAM->m_mapBRTPlayers.end())
						continue;

					m_pName[i]->SetTextClr(0xFFFFFFFF);
				}
				else
					m_pName[i]->SetTextClr(0xFFFF4500);

				CString strNAME = (*itBegin).second->m_strNAME;
				m_pName[i]->m_strText.Format("%s", strNAME);
				m_pKills[i]->m_strText.Format("%d", pTEAM->m_wTotalKills);
			}
			else
			{
				m_pName[i]->m_strText.Empty();
				m_pKills[i]->m_strText.Empty();
			}
		}
		else
		{
			m_pName[i]->m_strText.Empty();
			m_pKills[i]->m_strText.Empty();
		}
	}	
}

ITDetailInfoPtr	CBRTRanking::GetTInfoKey( const CPoint& pt )
{
	BYTE bIndex = (BYTE) T_INVALID;
	ITDetailInfoPtr pInfo(NULL);

	if(!IsVisible())
		return pInfo;

	for (BYTE i = 0; i < BR_RANK_COUNT; ++i)
	{
		if (m_pButtons[i]->HitTest(pt) ||
			m_pName[i]->HitTest(pt)	||
			m_pKills[i]->HitTest(pt) && 
			m_pName[i]->m_strText != NAME_NULL)
		{
			bIndex = i;
			break;
		}
	}

	if (bIndex == (BYTE) T_INVALID)
		return pInfo;

	if (bIndex >= m_vBRTRanking.size())
		return pInfo;

	LPBRTRANKING pTEAM = m_vBRTRanking[bIndex];
	if (!pTEAM)
		return pInfo;

	CString* strPlayer = new CString[pTEAM->m_mapBRTPlayers.size()];

	BYTE bPIndex = 0;
	if (pTEAM)
	{
		for (MAPBRTPLAYER::iterator it = pTEAM->m_mapBRTPlayers.begin(); it != pTEAM->m_mapBRTPlayers.end(); ++it)
		{
			strPlayer[bPIndex].Format("%s - %d kills | %d lifes", (*it).second->m_strNAME, (*it).second->m_wKills, (*it).second->m_bLifes);
			bPIndex++;
		}
	}

	CRect rc;
	GetComponentRect(&rc);

	pInfo = CTDetailInfoManager::NewBRTeamsToolTip("Players in this team", strPlayer, rc);

	return pInfo;
}