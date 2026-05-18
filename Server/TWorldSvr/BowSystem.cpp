/* Copyright(C) Games - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by Pavel Novák <tvfromzoe@seznam.cz>, February 2017
*/

#include "stdafx.h"
#include "BowSystem.h"
#include "TWorldSvr.h"
#include "TWorldSvrModule.h"

#define BOW_TEAM_COUNT 2
#define BOW_MAX_POINTS 10

CTBowSystem::CTBowSystem(WORD wMapID, BYTE bMinPlayersCount, BYTE bMaxNationDifference)
{
	m_wMapID = wMapID;
	m_bMinPlayersCount = bMinPlayersCount;
	m_bMaxNationDifference = bMaxNationDifference;
	m_bStatus = BS_PEACE;
	m_bRunning = FALSE;
	m_dwStart = 0;
	m_dwTick = 0;
	m_dwPeaceTick = 0;
	m_pBOWServer = NULL;
	m_bWinner = TCONTRY_N;
	m_vTIMES.clear();

	Release();
}

CTBowSystem::~CTBowSystem()
{
	if (m_pBOWServer)
	{
		delete m_pBOWServer;
		m_pBOWServer = NULL;
	}

	m_vTIMES.clear();
	Release();
}

void CTBowSystem::ConnectMap(CTServer* pServer)
{
	if (pServer)
		m_pBOWServer = pServer;
}

void CTBowSystem::Init()
{
	CTime curtime(CTime::GetCurrentTime());
	INT nCH = curtime.GetHour();
	INT nCM = curtime.GetMinute();
	INT nCS = curtime.GetSecond();
	DWORD dwCurrentCLT = nCH * 60 * 60 + nCM * 60 + nCS;

	sort(m_vTIMES.begin(), m_vTIMES.end());
	VECTORDWORD::iterator it = std::upper_bound(m_vTIMES.begin(), m_vTIMES.end(), dwCurrentCLT);
	if (it != m_vTIMES.end())
		m_dwStart = *it;
	else
		m_dwStart = (*m_vTIMES.begin());

	if (m_dwStart < dwCurrentCLT && it != m_vTIMES.end())
	{
		for (BYTE i = 0; i < m_vTIMES.size(); ++i)
		{
			if (m_vTIMES[i] > dwCurrentCLT)
			{
				m_dwStart = m_vTIMES[i];
				break;
			}
		}
	}

	CString strLog;
	strLog.Format("First time set to %d", m_dwStart);
	Log(strLog);
}

BYTE CTBowSystem::AddPlayerToQueue(DWORD dwCharID, DWORD dwKEY, BYTE bCountry, DWORD dwGuildID, BOOL Admin)
{
	if (bCountry > TCONTRY_C)
		return BOWREG_COUNTRY;

	if (m_bStatus == BS_PEACE || Admin)
	{
		if (m_mapBOWPLAYERS.empty() ||
			m_mapBOWPLAYERS.find(dwCharID) != m_mapBOWPLAYERS.end())
		{
			return BOWREG_FAIL;
		}

		if (bCountry > TCONTRY_C)
			return BOWREG_FAIL;

		DeletePlayerFromQueue(dwCharID, dwKEY);

		LPBOWPLAYER pPLAYER = new BOWPLAYER();
		pPLAYER->m_dwCharID = dwCharID;
		pPLAYER->m_dwKEY = dwKEY;

		BYTE PlayerCount[2] = { 0 };
		for (MAPBOWPLAYER::iterator itPlayer = m_mapBOWPLAYERS.begin(); itPlayer != m_mapBOWPLAYERS.end(); ++itPlayer)
			PlayerCount[(*itPlayer).second->m_bCountry]++;

		pPLAYER->m_bCountry = PlayerCount[TCONTRY_C] < PlayerCount[TCONTRY_D] ? TCONTRY_C : TCONTRY_D;
		m_mapBOWPLAYERS.insert(MAPBOWPLAYER::value_type(dwCharID, pPLAYER));

		MAPBOWPLAYER mapTemp; mapTemp.clear();
		mapTemp.insert(MAPBOWPLAYER::value_type(dwCharID, pPLAYER));

		_AtlModule.TeleportBOWPlayer(mapTemp, TRUE);

		CString strLog;
		strLog.Format("Player joined after the normal time. Current Player Count : %d, Valorian : %d, Derion : %d",
			m_mapBOWPLAYERS.size(),
			PlayerCount[TCONTRY_D],
			PlayerCount[TCONTRY_C]);

		Log(strLog);

		return BOWREG_FAIL + 1;
	}

	if (m_bStatus != BS_ALARM)
		return BOWREG_FAIL;

	if (dwGuildID)
	{
		if (m_mapBOWREG.find(dwCharID) != m_mapBOWREG.end())
			return BOWREG_FAIL;

		MAPBOWGUILDPLAYER::iterator it = m_mapGuildMember.find(dwGuildID);
		if (it == m_mapGuildMember.end())
		{
			VBOWPLAYER vMember;
			BOWTEMPPLAYER pPLAYER;

			pPLAYER.m_dwCharID = dwCharID;
			pPLAYER.m_dwKEY = dwKEY;
			pPLAYER.m_bCountry = bCountry;

			vMember.push_back(pPLAYER);
			m_mapGuildMember.insert(MAPBOWGUILDPLAYER::value_type(dwGuildID, vMember));
		}
		else
		{
			for (size_t i = 0; i < (*it).second.size(); ++i)
				if ((*it).second[i].m_dwCharID == dwCharID)
					return BOWREG_FAIL;

			BOWTEMPPLAYER pPLAYER;

			pPLAYER.m_dwCharID = dwCharID;
			pPLAYER.m_dwKEY = dwKEY;
			pPLAYER.m_bCountry = bCountry;

			(*it).second.push_back(pPLAYER);
		}

		return BOWREG_SUCCESS;
	}
	else
	{
		MAPBOWTEMPPLAYER::iterator finder = m_mapBOWREG.find(dwCharID);
		if (finder == m_mapBOWREG.end())
		{
			LPBOWTEMPPLAYER pPLAYER = new BOWTEMPPLAYER();
			pPLAYER->m_dwCharID = dwCharID;
			pPLAYER->m_dwKEY = dwKEY;
			pPLAYER->m_bCountry = bCountry;

			m_mapBOWREG.insert(MAPBOWTEMPPLAYER::value_type(dwCharID, pPLAYER));
			return BOWREG_SUCCESS;
		}
	}

	return BOWREG_ALREADYINQUEUE;
}

BYTE CTBowSystem::DeletePlayerFromQueue(DWORD dwCharID, DWORD dwKEY)
{
	if (m_bStatus != BS_ALARM)
		return BOWREG_FAIL;

	MAPBOWGUILDPLAYER::iterator itGuild;
	for (itGuild = m_mapGuildMember.begin(); itGuild != m_mapGuildMember.end(); ++itGuild)
	{
		for (size_t i = 0; i < (*itGuild).second.size(); ++i) {
			if ((*itGuild).second[i].m_dwCharID == dwCharID && (*itGuild).second[i].m_dwKEY == dwKEY)
			{
				(*itGuild).second.erase((*itGuild).second.begin() + i);
				return BOWREG_SUCCESS;
			}
		}
	}
	MAPBOWTEMPPLAYER::iterator finder = m_mapBOWREG.find(dwCharID);
	if (finder != m_mapBOWREG.end())
	{
		if ((*finder).second->m_dwKEY == dwKEY)
		{
			delete (*finder).second;
			m_mapBOWREG.erase(finder);
			return BOWREG_SUCCESS;
		}
	}

	return BOWREG_FAIL;
}

DWORD CTBowSystem::CreateMatch()
{
	m_bWinner = TCONTRY_N;

	CString strLog;
	BYTE bPlayerCount[TCONTRY_B] = { 0 };

	MAPBOWTEMPPLAYER::iterator itPlayers;
	MAPBOWTEMPPLAYER mapFirstTeam, mapSecondTeam;
	mapFirstTeam.clear();
	mapSecondTeam.clear();

	MAPBOWGUILDPLAYER::iterator itGuild;
	for (itGuild = m_mapGuildMember.begin(); itGuild != m_mapGuildMember.end(); ++itGuild)
	{
		for (size_t i = 0; i < (*itGuild).second.size(); ++i)
		{
			BYTE bCountry = (*itGuild).second.front().m_bCountry;

			LPBOWTEMPPLAYER pPLAYER = new BOWTEMPPLAYER();
			pPLAYER->m_dwCharID = (*itGuild).second[i].m_dwCharID;
			pPLAYER->m_dwKEY = (*itGuild).second[i].m_dwKEY;
			pPLAYER->m_bCountry = (*itGuild).second[i].m_bCountry;

			if (bCountry == TCONTRY_D)
				mapFirstTeam.insert(MAPBOWTEMPPLAYER::value_type((*itGuild).second[i].m_dwCharID, pPLAYER));
			else
				mapSecondTeam.insert(MAPBOWTEMPPLAYER::value_type((*itGuild).second[i].m_dwCharID, pPLAYER));
		}
	}

	BYTE bMovedFromGuilds = 0;
	if (abs((BYTE) mapFirstTeam.size() - (BYTE) mapSecondTeam.size()) > (BYTE) m_mapBOWREG.size() + m_bMaxNationDifference)
	{
		MAPBOWTEMPPLAYER* mapSmallerTeam;
		MAPBOWTEMPPLAYER* mapBiggerTeam;

		mapSmallerTeam = mapFirstTeam.size() < mapSecondTeam.size() ?
			&mapFirstTeam : &mapSecondTeam;
		mapBiggerTeam = mapSmallerTeam == &mapFirstTeam ? &mapSecondTeam : &mapFirstTeam;

		BYTE bSmallestDifference = 255;
		BYTE bDifference;
		MAPBOWGUILDPLAYER::iterator itGuildToMoveToBalance;

		itGuild = m_mapGuildMember.begin();
		while (itGuild != m_mapGuildMember.end())

		{
			if (!mapSmallerTeam->size())
				break;


			if ((*itGuild).second.front().m_bCountry == mapSmallerTeam->begin()->second->m_bCountry)
			{
				++itGuild;
				continue;
			}




			bDifference = abs(((BYTE) mapSmallerTeam->size() + (BYTE) (*itGuild).second.size()) - ((BYTE) mapBiggerTeam->size() - (BYTE) (*itGuild).second.size()));
			if (bDifference < bSmallestDifference)


			{
				itGuildToMoveToBalance = itGuild;
				bSmallestDifference = bDifference;
			}






			++itGuild;
		}






		if (itGuildToMoveToBalance != m_mapGuildMember.end())
		{
			bMovedFromGuilds = (BYTE) (*itGuildToMoveToBalance).second.size();
			CString xd;
			xd.Format("MovedFromGuilds : %d", bMovedFromGuilds);
			Log(xd);



			for (size_t k = 0; k < (*itGuildToMoveToBalance).second.size(); ++k)

			{
				for (MAPBOWTEMPPLAYER::iterator itMoving = mapBiggerTeam->begin(); itMoving != mapBiggerTeam->end(); ++itMoving)


				{
					if ((*itGuildToMoveToBalance).second[k].m_dwCharID == (*itMoving).second->m_dwCharID)

					{


						mapSmallerTeam->insert(MAPBOWTEMPPLAYER::value_type((*itMoving).first, (*itMoving).second));
						mapBiggerTeam->erase(itMoving);

						CString x;
						x.Format("Smalelr : %d, Bigger : %d", mapSmallerTeam->size(), mapBiggerTeam->size());

						Log(x);

					}
				}
			}
		}
	}

	for (itPlayers = m_mapBOWREG.begin(); itPlayers != m_mapBOWREG.end(); ++itPlayers)
	{
		LPBOWTEMPPLAYER pPLAYER = new BOWTEMPPLAYER();
		pPLAYER->m_dwCharID = (*itPlayers).second->m_dwCharID;
		pPLAYER->m_dwKEY = (*itPlayers).second->m_dwKEY;
		pPLAYER->m_bCountry = (*itPlayers).second->m_bCountry;

		if (mapFirstTeam.size() < mapSecondTeam.size())
			mapFirstTeam.insert(MAPBOWTEMPPLAYER::value_type((*itPlayers).second->m_dwCharID, pPLAYER));
		else
			mapSecondTeam.insert(MAPBOWTEMPPLAYER::value_type((*itPlayers).second->m_dwCharID, pPLAYER));
	}

	BYTE bDifference = abs((BYTE)mapFirstTeam.size() - (BYTE)mapSecondTeam.size());
	if (bDifference > m_bMaxNationDifference)
	{
		BYTE bIndex = 0;
		MAPBOWTEMPPLAYER* mapMoveFrom;
		MAPBOWTEMPPLAYER* mapMoveTo;
		mapMoveFrom = mapFirstTeam.size() > mapSecondTeam.size() ? &mapFirstTeam : &mapSecondTeam;
		mapMoveTo = mapMoveFrom == &mapFirstTeam ? &mapSecondTeam : &mapFirstTeam;

		for (MAPBOWTEMPPLAYER::iterator itMove = mapMoveFrom->begin(); itMove != mapMoveFrom->end(); ++itMove)
		{
			mapMoveTo->insert(MAPBOWTEMPPLAYER::value_type((*itMove).first, (*itMove).second));
			mapMoveFrom->erase(itMove);

			bIndex++;
			if (bIndex == BYTE(bDifference / 2))
				break;
		}
	}

	for (itPlayers = mapFirstTeam.begin(); itPlayers != mapFirstTeam.end(); ++itPlayers)
	{
		LPBOWPLAYER pPLAYER = new BOWPLAYER();

		pPLAYER->m_dwCharID = (*itPlayers).second->m_dwCharID;
		pPLAYER->m_dwKEY = (*itPlayers).second->m_dwKEY;
		pPLAYER->m_bCountry = (*itPlayers).second->m_bCountry;

		if (pPLAYER->m_bCountry != TCONTRY_D)
			pPLAYER->m_bCountry = TCONTRY_D;

		m_mapBOWPLAYERS.insert(MAPBOWPLAYER::value_type((*itPlayers).second->m_dwCharID, pPLAYER));
		delete (*itPlayers).second;
	}

	for (itPlayers = mapSecondTeam.begin(); itPlayers != mapSecondTeam.end(); ++itPlayers)
	{
		LPBOWPLAYER pPLAYER = new BOWPLAYER();

		pPLAYER->m_dwCharID = (*itPlayers).second->m_dwCharID;
		pPLAYER->m_dwKEY = (*itPlayers).second->m_dwKEY;
		pPLAYER->m_bCountry = (*itPlayers).second->m_bCountry;

		if (pPLAYER->m_bCountry != TCONTRY_C)
			pPLAYER->m_bCountry = TCONTRY_C;

		m_mapBOWPLAYERS.insert(MAPBOWPLAYER::value_type((*itPlayers).second->m_dwCharID, pPLAYER));
		delete (*itPlayers).second;
	}

	for (BYTE i = 0; i < BOW_TEAM_COUNT; ++i)
		m_bPoints[i] = BOW_MAX_POINTS / BOW_TEAM_COUNT;

	_AtlModule.TeleportBOWPlayer(m_mapBOWPLAYERS, TRUE);

	strLog.Format("Participant count : %d, Guilds : %d, Moved Guild Players : %d", m_mapBOWPLAYERS.size(), m_mapGuildMember.size(), bMovedFromGuilds);
	Log(strLog);

	return BOWMATCH_SUCCESS;
}

void CTBowSystem::SetDurations(DWORD dwAlarmDur, DWORD dwBuyTimeDur, DWORD dwBattleDur)
{
	m_dwAlarmDur = dwAlarmDur;
	m_dwBuyTimeDur = dwBuyTimeDur;
	m_dwBattleDur = dwBattleDur;
}

void CTBowSystem::SetStatus(BYTE bStatus, DWORD dwSecond)
{
	if (!m_pBOWServer || !m_bRunning)
		return;

	static BOOL SentEnd = FALSE;
	if (bStatus == BS_BODPEACE && m_bStatus != BS_BODPEACE)
		m_dwPeaceTick = GetTickCount();

	if (m_dwPeaceTick)
	{
		if (!SentEnd && m_pBOWServer)
		{
			m_bWinner = BOWWINNER_TIE;
			if (m_bPoints[TCONTRY_D] > m_bPoints[TCONTRY_C])
				m_bWinner = BOWWINNER_DEFUGEL;
			else if (m_bPoints[TCONTRY_C] > m_bPoints[TCONTRY_D])
				m_bWinner = BOWWINNER_CRAXION;
			else if (m_bPoints[TCONTRY_D] == m_bPoints[TCONTRY_C])
				m_bWinner = BOWWINNER_TIE;

			m_pBOWServer->SendMW_BOWCOMMANDEXEC_REQ(BOW_END);
			SentEnd = TRUE;
		}

		bStatus = m_bStatus = BS_BODPEACE;

		DWORD dwLeft = GetTickCount() - m_dwPeaceTick;
		dwSecond = 12 - (dwLeft / 1000);
		if (dwLeft >= 12000)
		{
			EndBattle();
			SentEnd = FALSE;
		}

		_AtlModule.BOWNotify(
			bStatus,
			dwSecond,
			m_bPoints[TCONTRY_D],
			m_bPoints[TCONTRY_C]);

		return;
	}

	_AtlModule.BOWNotify(
		bStatus,
		dwSecond,
		m_bPoints[TCONTRY_D],
		m_bPoints[TCONTRY_C]);

	switch (bStatus)
	{
	case BS_BATTLE:
	

		{
			if (m_bStatus != bStatus && m_pBOWServer)
			{
				m_pBOWServer->SendMW_BOWCOMMANDEXEC_REQ(BOW_START);
				_AtlModule.NotifyBOWNonQueuedPlayers();
			}
		}

		break;
	case BS_PEACE:

		{
			if (!m_mapBOWPLAYERS.empty() || bStatus == m_bStatus)
				return;



			DWORD dwResult = CreateMatch();

			for (MAPBOWTEMPPLAYER::iterator itReg = m_mapBOWREG.begin(); itReg != m_mapBOWREG.end(); ++itReg)
				delete (*itReg).second;
			m_mapBOWREG.clear();
			m_mapGuildMember.clear();

			if (dwResult != BOWMATCH_SUCCESS)

			{
				SetNextTime();
				Release();
				Log("Match initialization has failed.");

				_AtlModule.BOWNotify(
					BS_NORMAL, 

					0,
					m_bPoints[TCONTRY_D], 
					m_bPoints[TCONTRY_C]);

				return;
			}
			else
				Log("Successfuly initialized match!");
		}



		break;
	}

	m_dwTick = dwSecond;
	m_bStatus = bStatus;
}

void CTBowSystem::UpdatePoints(BYTE bCountry)
{
	switch (bCountry)
	{
	case TCONTRY_D:



		{
			m_bPoints[TCONTRY_D]++;
			if (m_bPoints[TCONTRY_C] == BOW_MAX_POINTS / BOW_MAX_POINTS)
			{
				m_bPoints[TCONTRY_C] = 0;
				m_dwPeaceTick = GetTickCount();
			}
			else
				if ((INT) m_bPoints[TCONTRY_C] - 1 > 0)
					m_bPoints[TCONTRY_C]--;
		}




		break;
	case TCONTRY_C:



		{
			m_bPoints[TCONTRY_C]++;
			if (m_bPoints[TCONTRY_D] == BOW_MAX_POINTS / BOW_MAX_POINTS)
			{
				m_bPoints[TCONTRY_D] = 0;
				m_dwPeaceTick = GetTickCount();
			}
			else
				if ((INT) m_bPoints[TCONTRY_D] - 1 > 0)
					m_bPoints[TCONTRY_D]--;
		}




		break;
	}

	_AtlModule.BOWNotify(
		m_bStatus, 
		m_dwTick,
		m_bPoints[TCONTRY_D], 
		m_bPoints[TCONTRY_C]);
}

void CTBowSystem::EndBattle()
{
	if (!m_pBOWServer)
		return;

	_AtlModule.TeleportBOWPlayer(m_mapBOWPLAYERS, FALSE, m_bWinner);
	SetNextTime();
	Release();
}

void CTBowSystem::SetNextTime()
{
	VECTORDWORD::iterator it = std::upper_bound(m_vTIMES.begin(), m_vTIMES.end(), m_dwStart);
	if (it != m_vTIMES.end())
		m_dwStart = *it;
	else
		m_dwStart = (*m_vTIMES.begin());

	CString strLog; strLog.Format("Setting next time to : %d", m_dwStart);
	Log(strLog);
}

void CTBowSystem::ReleaseSinglePlayer(DWORD dwCharID, DWORD dwKEY)
{
	LPBOWPLAYER Player = FindPlayer(dwCharID);
	if (Player && Player->m_dwKEY == dwKEY && m_bWinner != TCONTRY_N)
		_AtlModule.TeleportBOWPlayer(dwCharID, dwKEY, m_bWinner);
}

void CTBowSystem::Release()
{
	for (BYTE i = 0; i < BOW_TEAM_COUNT; ++i)
		m_bPoints[i] = BOW_MAX_POINTS / BOW_TEAM_COUNT;

	m_bRunning = FALSE;
	m_bStatus = BS_NORMAL;
	m_dwTick = 0;
	m_dwPeaceTick = 0;

	for (MAPBOWTEMPPLAYER::iterator itReg = m_mapBOWREG.begin(); itReg != m_mapBOWREG.end(); ++itReg)
		delete (*itReg).second;

	for (MAPBOWPLAYER::iterator itPlayer = m_mapBOWPLAYERS.begin(); itPlayer != m_mapBOWPLAYERS.end(); ++itPlayer)
		delete (*itPlayer).second;

	m_mapBOWREG.clear();
	m_mapBOWPLAYERS.clear();
	m_mapGuildMember.clear();
}

LPBOWPLAYER CTBowSystem::FindPlayer(DWORD dwCharID)
{
	MAPBOWPLAYER::iterator finder = m_mapBOWPLAYERS.find(dwCharID);
	if (finder != m_mapBOWPLAYERS.end())
		return (*finder).second;

	return NULL;
}

/*============ LOGS ============*/
void CTBowSystem::Log(CString strLog)
{
	FILE* pLog = fopen("C:\\TServices_GSP\\BOWDebug.log", "a+");
	if (!pLog)
		pLog = fopen("C:\\TServices_GSP\\BOWDebug.log", "w+");
	if (!pLog)
		return;

	CString strText;
	CTime tTime(CTime::GetCurrentTime());
	strText.Format("[BOW-WORLD] %s - %s\n", tTime.Format("%c"), strLog);

	fprintf(pLog, strText);
	fclose(pLog);
}