#pragma once

#define BR_TEAM_MEMBER		(BYTE) 3
#define TIME_IMAGE_COUNT	(BYTE) (2*2)

class CBRTeamDlg : public CTClientUIBase
{
public:
	TButton* m_pTeamButton[BR_TEAM_MEMBER];
	TButton* m_pTeamInvite[BR_TEAM_MEMBER - 1];
	TButton* m_pTeamLeave[BR_TEAM_MEMBER - 1];

	TComponent* m_pInTeamMarker[BR_TEAM_MEMBER];
	TComponent* m_pTeamName[BR_TEAM_MEMBER];
	TComponent* m_pTeamChief;

	TGauge* m_pTeamGauge[BR_TEAM_MEMBER];

	TComponent* m_pPlaceHolder;
	TComponent* m_pText;

	TButton* m_pMapList;
	TButton* m_pModeList;
	TButton* m_pStart;

	TList* m_pList;
	TList* m_pList2;

	TImageList* m_pTime[TIME_IMAGE_COUNT];
private:
	std::vector<BRPREMADEPLAYER> m_vTeam;
	BYTE m_bIsChief;
	BYTE m_bTeamReady;
	BYTE m_Mode;
	BOOL m_bMapListVisible;
	BOOL m_bModeListVisible;
	CString m_strSelectedMap;
public:
	void LeaveTeam();
	void UpdateTeam(std::vector<BRPREMADEPLAYER>& vTeam, BYTE bTeamReady);
	void SetTime(DWORD dwTime);
	void LoadMapList(const std::vector<CString>& vMaps);

	BYTE GetTeamReady() { return m_bTeamReady; }
public:
	virtual HRESULT Render(DWORD dwTickCount);
	virtual BOOL CanWithItemUI();
	virtual void ShowComponent(BOOL bVisible);
	virtual void OnLButtonUp(UINT nFlags, CPoint pt);
public:
	CBRTeamDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CBRTeamDlg();
};