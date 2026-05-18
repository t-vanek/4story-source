#pragma once

enum BR_CLASS
{
	CLASS_EYE = 0,
	CLASS_ATTACK,
	CLASS_SUPPORT
};

enum BR_TYPE_TEAM
{
	BR_SOLO = 0,
	BR_TEAM,
};

struct tagBRMAP
{
	WORD m_wMapID;
	CString m_strName;
};

typedef struct tagBRMAP BRMAP;

class CBRSystem
{
public:
	MAPBRTEAMS m_mapPremadeTeam;
	MAPBRTEAMS m_mapBRTeam;
	MAPBRTEMPPLAYER m_mapBRREG;
	MAPDWORD m_mapMapVote;
	MAPDWORD m_mapVoteMode;

	std::vector<BRMAP> m_vBRMap;
	VECTORBYTE m_vBattleType;
public:
	DWORD m_dwAlarmDur;
	DWORD m_dwBuyTimeDur;
	DWORD m_dwBattleDur;
	DWORD m_dwStart;
	DWORD m_dwTick;
	DWORD m_dwPeaceTick;

	BYTE m_bStatus;
	BYTE m_bType;
	BYTE m_bMinPlayerCount;
	BOOL m_bRunning;
	BYTE m_Mode;

	VECTORDWORD m_vTIMES;
	CTServer* m_pBRServer;
private:
	WORD m_wMapID;
private:
	void Release();
public:
	void ConnectMap(CTServer* pServer);
	void Init();
	void SetNextTime();
	void SetStatus(BYTE bStatus, DWORD dwSecond);
	void JoinPremadeTeam(DWORD dwChiefID, 
		DWORD dwChiefKEY,
		BYTE bChiefClass, 
		CString strChiefName, 
		DWORD dwMateID,
		DWORD dwMateKEY, 
		BYTE bMateClass, 
		CString strMateName);

	BOOL FindPlayerInPremade(DWORD dwCharID);
	BOOL FindPlayerInTeam(DWORD dwCharID);
	LPTBRPLAYERS FindPlayerPointerInTeam(DWORD dwCharID);
	BYTE GetPremadePlayerCountByChief(DWORD dwChiefID);
	BYTE ErasePlayerFromQueue(DWORD dwCharID, DWORD dwKEY);
	void ErasePlayerFromPremade(DWORD dwCharID);
	void UpdatePlayerQueue();
	BYTE FlagPlayerReady(DWORD dwCharID, DWORD dwKEY);
	BYTE FlagTeamReady(DWORD dwChiefID);
	DWORD GetChiefIDByMateID(DWORD dwCharID);

	BYTE AddPlayerToQueue(DWORD dwCharID, DWORD dwKEY, BYTE bClass, CString strNAME);
	BYTE CreateMatch();
	void SwitchType();
	void AddMap(WORD wMapID, CString strName);
	void VoteForMap(DWORD dwUserID, CString strMap);
	void VoteForMode(DWORD dwUserID, BYTE Mode);
	WORD GetMapID() { return m_wMapID; }
	void OnEnd();
	void ReleaseSinglePlayer(DWORD dwCharID, DWORD dwKEY);
public:
	static void Log(CString strLog);
public:
	CBRSystem(DWORD dwAlarmDur, DWORD dwBuyTimeDur, DWORD dwBattleDur, BYTE bMinPlayerCount);
	~CBRSystem(void);
};
