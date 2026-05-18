#pragma once
#include "BRTRanking.h"
#define BR_RANKING_UI_SIZE       (4)

class CBRRankingDlg : public CTClientUIBase
{
public :
	TComponent* m_pNRanking[BR_RANKING_UI_SIZE];
	TComponent* m_pName[BR_RANKING_UI_SIZE];
	TComponent* m_pServer[BR_RANKING_UI_SIZE];
	TComponent* m_pKills[BR_RANKING_UI_SIZE];
	TComponent* m_pLifes[BR_RANKING_UI_SIZE];
	TButton*    m_pExit;

	std::vector<LPBRTPLAYER> m_vBRRANK;
public:
	void ResetRank();
	void CloneTRanking(LPBRTPLAYER Player);
	void UpdateRanking(DWORD dwCharID, WORD wKills, BYTE bLifes);
	void ReleaseRanking();
public:
	CBRRankingDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CBRRankingDlg();
};
