#pragma once

class CTBowSystem
{
public:
	BOOL m_bRunning;
	BYTE m_bStatus;
	BYTE m_bWinner;
	BYTE m_bWinnerCountry;
	
	DWORD m_dwStart;
	DWORD m_dwAlarmDur;
	DWORD m_dwBuyTimeDur;
	DWORD m_dwBattleDur;
	DWORD m_dwTick;
	DWORD m_dwPeaceTick;

	MAPBOWPLAYER m_mapBOWPLAYERS;
	MAPBOWTEMPPLAYER m_mapBOWREG;
	VECTORDWORD m_vTIMES;
	
	MAPBOWGUILDPLAYER m_mapGuildMember;
	CTServer* m_pBOWServer;
protected:
	WORD m_wMapID;
	BYTE m_bMinPlayersCount;
	BYTE m_bMaxNationDifference;

	BYTE m_bPoints[2];
public:
	void ConnectMap(CTServer* pServer);
	void Init();
	BYTE AddPlayerToQueue(DWORD dwCharID, DWORD dwKEY, BYTE bCountry, DWORD dwGuildID, BOOL Admin = FALSE);
	DWORD CreateMatch();
	BYTE DeletePlayerFromQueue(DWORD dwCharID, DWORD dwKEY);
	LPBOWPLAYER FindPlayer(DWORD dwCharID);
	void SetDurations(DWORD dwAlarmDur, DWORD dwBuyTimeDur, DWORD dwBattleDur);
	void SetStatus(BYTE bStatus, DWORD dwSecond);
	void UpdatePoints(BYTE bCountry);
	void ReleaseSinglePlayer(DWORD dwCharID, DWORD dwKEY);
	void EndBattle();
	void SetNextTime();
	void Release();
public:
	static void Log(CString strLog);
public:
	CTBowSystem(WORD wMapID, BYTE bMinPlayersCount, BYTE bMaxNationDifference);
	~CTBowSystem(void);
};
