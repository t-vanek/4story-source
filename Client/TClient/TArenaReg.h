#pragma once

struct ArenaPlayer
{
	CString m_ChiefName;
	BYTE m_Level;
	BYTE m_Country;
	BYTE m_Wins;
};

#define ARENA_MAX_TEAM		2

class CTArenaRegDlg : public CTClientUIBase
{
private :

	TButton* m_pUComp;
	TButton* m_pDComp;

	TButton* m_pComps[5];

	TComponent* m_pGroupUName;
	TComponent* m_pGroupDName;

	TComponent* m_pUWinCount;
	TComponent* m_pDWinCount;

	TComponent* m_pUPosition;
	TComponent* m_pDPosition;

	TComponent* m_pULevel;
	TComponent* m_pDLevel;

	TImageList* m_pGroupUC;
	TImageList* m_pGroupDC;

	TComponent* m_pQUEUEName[5];
	TImageList* m_pQUEUEC[5];
	TComponent* m_pWinCounts[5];
	TComponent* m_pLevels[5];

	TScroll* m_pTSCROLL;

	ArenaPlayer* m_ArenaPlayer[ARENA_MAX_TEAM];
	std::vector<ArenaPlayer> m_Queue;

public :
	

	BYTE m_bQueueCount;
	INT m_nListTop;

	CTArenaRegDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CTArenaRegDlg();

	void AddTeam(CString ChiefName, BYTE Level, BYTE Country, BYTE Wins);

	void Update();

	HRESULT Render(DWORD dwTickCount);
	void Release();
	void AddPlayersQueue(CString strName);
	void UpdateScrollPosition();
	void AddPlayers(DWORD dwFCharID, BYTE bFCountry, BYTE bFLevel, CString strFName, BYTE bFWT, DWORD dwSID, BYTE bSC, BYTE bSLevel, CString strSName, BYTE bSWT);

	virtual void OnNotify( DWORD from, WORD msg, LPVOID param );
	virtual BOOL DoMouseWheel( UINT nFlags, short zDelta, CPoint pt);
	virtual void OnLButtonDown(UINT nFlags, CPoint point);
};
