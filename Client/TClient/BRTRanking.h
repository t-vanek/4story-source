#pragma once
#define  BR_RANK_COUNT       7

class CBRTRanking : public CTClientUIBase
{
public:
	TComponent* m_pName    [ BR_RANK_COUNT ];
	TComponent* m_pKills   [ BR_RANK_COUNT ];
	TButton*    m_pButtons [ BR_RANK_COUNT ];
public:
	VECTORBRTRANKING m_vBRTRanking;
public:
	LPBRTRANKING FindTeam(DWORD dwTeamID);
	void AddTeam(DWORD dwTeamID, const MAPBRTPLAYER& mapTeamPlayer);
	WORD RecalcTotalKills(DWORD dwTeamID);
	void UpdatePlayer(DWORD dwTeamID, DWORD dwCharID, WORD wKills);
	void UpdateUI();
	void ReleaseTeams();

public:
	CBRTRanking( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CBRTRanking();
	virtual BOOL	CanWithItemUI();
	virtual ITDetailInfoPtr	GetTInfoKey( const CPoint& pt );

	//void OnLButtonUp(UINT nFlags, CPoint pt);
};