#pragma once
#define BOW_RANK_COUNT      (4)

class CTBowRankingDlg : public CTClientUIBase
{
public :
	TComponent* m_pNRanking[BOW_RANK_COUNT];
	TComponent* m_pName[BOW_RANK_COUNT];
	TComponent* m_pServer[BOW_RANK_COUNT];
	TComponent* m_pBP[BOW_RANK_COUNT];
	TComponent* m_pBB[BOW_RANK_COUNT];
	TButton*    m_pExit;
	TImageList* m_pCountry[BOW_RANK_COUNT];

	TImageList* m_pGCountry[BOW_RANK_COUNT];
	TComponent* m_pGNRanking[BOW_RANK_COUNT];
	TComponent* m_pGBB[BOW_RANK_COUNT];
	TComponent* m_pGTP[BOW_RANK_COUNT];
	TComponent* m_pGServer[BOW_RANK_COUNT];
	TComponent* m_pGName[BOW_RANK_COUNT];

	VTBOWRANK		m_vTBODRANK;
	VTBOWGUILD		m_vTGUILDRANK;

	void ResetRank();
	static BOOL Sorter( CTBowRank* pLevy, CTBowRank* pPravy );
	static BOOL SortGuilds( CTBowGRank* pLevy, CTBowGRank* pPravy );
	void ReleaseData();
public:
	CTBowRankingDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CTBowRankingDlg();
};
