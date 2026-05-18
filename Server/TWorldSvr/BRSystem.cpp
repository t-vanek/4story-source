/* Copyright (C) Games - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Pavel Novák <tvfromzoe@seznam.cz>, September 2017
 */

#include "stdafx.h"
#include "BRSystem.h"
#include "TWorldSvr.h"
#include "TWorldSvrModule.h"

#define BR_PARTY_SIZE(x) BYTE(x == BR_3V3 ? 3 : 2)
#define DEFAULT_BR_MAP      (WORD) 710 //700 yesode

CBRSystem::CBRSystem(DWORD dwAlarmDur, DWORD dwBuyTimeDur, DWORD dwBattleDur, BYTE bMinPlayerCount)
{
	m_bStatus = BS_NORMAL;
	m_dwStart = 0;
	m_dwAlarmDur = dwAlarmDur;
	m_dwBuyTimeDur = dwBuyTimeDur;
	m_dwBattleDur = dwBattleDur;
	m_bMinPlayerCount = bMinPlayerCount;
	m_dwTick = 0;
	m_bType = BR_TEAM;
	m_bRunning = FALSE;
	m_wMapID = DEFAULT_BR_MAP;
	m_dwPeaceTick = 0;

	m_pBRServer = NULL;
	m_vTIMES.clear();
	m_vBattleType.clear();

	AddMap(706, "Hod");
	AddMap(710, "Blonea");
	AddMap(703, "Colossus");
	AddMap(708, "Tyconteroga");

	m_Mode = BR_3V3;

	Release();
}

CBRSystem::~CBRSystem()
{
	Release();

	if (m_pBRServer)
	{
		delete m_pBRServer;
		m_pBRServer = NULL;
	}
}

void CBRSystem::Init()
{
	CTime curtime(CTime::GetCurrentTime());
	INT nCH = curtime.GetHour();
	INT nCM = curtime.GetMinute();
	INT nCS = curtime.GetSecond();
	DWORD dwCurrentCLT = nCH * 60 * 60 + nCM * 60 + nCS;

	sort(m_vTIMES.begin(), m_vTIMES.end());

	for (BYTE i = 0; i < m_vTIMES.size(); ++i)
	{
		if (i % 2 == 0)
			m_vBattleType.push_back(BR_TEAM);
		else
			m_vBattleType.push_back(BR_SOLO);
	}

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

	BYTE bBattleIndex = 0;
	for (BYTE i = 0; i < m_vTIMES.size(); ++i)
		if (m_vTIMES[i] == m_dwStart)
		{
			bBattleIndex = i;
			break;
		}

	if (bBattleIndex < m_vBattleType.size())
		m_bType = m_vBattleType[bBattleIndex];
	else
		m_bType = BR_TEAM;

	CString strLog;
	strLog.Format("First time set to %d, Type : %s", m_dwStart, m_bType == BR_TEAM ? "Team" : "Solo");
	Log(strLog);
}

void CBRSystem::SetNextTime()
{
	VECTORDWORD::iterator it = std::upper_bound(m_vTIMES.begin(), m_vTIMES.end(), m_dwStart);
	if (it != m_vTIMES.end())
		m_dwStart = *it;
	else
		m_dwStart = (*m_vTIMES.begin());

	BYTE bBattleIndex = 0;
	for (BYTE i = 0; i < m_vTIMES.size(); ++i)
		if (m_vTIMES[i] == m_dwStart)
		{
			bBattleIndex = i;
			break;
		}

	if (bBattleIndex < m_vBattleType.size())
		m_bType = m_vBattleType[bBattleIndex];
	else
		m_bType = BR_TEAM;

	CString strLog; 
	strLog.Format("Setting next time to : %d, Type : %s", m_dwStart, m_bType == BR_TEAM ? "Team" : "Solo");
	Log(strLog);
}

void CBRSystem::ConnectMap(CTServer* pServer)
{
	m_pBRServer = pServer;
}

void CBRSystem::SetStatus(BYTE bStatus, DWORD dwSecond)
{
	if (!m_pBRServer)
		return;

	static BOOL SentEnd = FALSE;
	if (bStatus == BS_BODPEACE && m_bStatus != BS_BODPEACE)
		m_dwPeaceTick = GetTickCount();

	if (m_dwPeaceTick)
	{
		if (!SentEnd && m_pBRServer)
		{
			m_pBRServer->SendMW_BRCOMMANDEXEC_REQ(BR_END);
			SentEnd = TRUE;
		}

		bStatus = m_bStatus = BS_BODPEACE;

		DWORD dwLeft = GetTickCount() - m_dwPeaceTick;
		dwSecond = 12 - (dwLeft / 1000);
		if (dwLeft >= 12000)
		{
			OnEnd();
			SentEnd = FALSE;
		}

		_AtlModule.BRNotify(
			bStatus, 
			dwSecond,
			m_bType);

		return;
	}

	_AtlModule.BRNotify(
		bStatus, 
		dwSecond,
		m_bType);

	switch (bStatus)
	{
	case BS_BATTLE:
		{
			if (m_bStatus != bStatus && m_pBRServer)
			{
				for (MAPBRTEMPPLAYER::iterator itReg = m_mapBRREG.begin(); itReg != m_mapBRREG.end(); ++itReg)
					delete (*itReg).second;
				m_mapBRREG.clear();

				m_pBRServer->SendMW_BRCOMMANDEXEC_REQ(BR_START);
			}
		}
		break;
	case BS_PEACE:
		{
			if (!m_mapBRTeam.empty() || bStatus == m_bStatus)
				return;

			BYTE bResult = CreateMatch();
			if (bResult)
				Log("Initialized match!");
			else
			{
				SetNextTime();
				Release();

				_AtlModule.BRNotify(
					bStatus, 
					dwSecond,
					m_bType);

				Log("Initialization has failed.");
				return;
			}
		}
		break;
	}

	m_dwTick = dwSecond;
	m_bStatus = bStatus;
}

BYTE CBRSystem::AddPlayerToQueue(DWORD dwCharID, DWORD dwKEY, BYTE bClass, CString strNAME)
{
	if (m_bStatus != BS_ALARM/* && m_bStatus != BS_PEACE*/)
		return BOWREG_FAIL;

	MAPBRTEMPPLAYER::iterator finder = m_mapBRREG.find(dwCharID);
	if (finder != m_mapBRREG.end() || FindPlayerInPremade(dwCharID))
		return BOWREG_ALREADYINQUEUE;

	LPBRTEMPPLAYER pPLAYER = new BRTEMPPLAYER();
	pPLAYER->m_dwCharID = dwCharID;
	pPLAYER->m_dwKEY = dwKEY;
	pPLAYER->m_bClass = bClass;
	pPLAYER->m_strNAME = strNAME;
	m_mapBRREG.insert(MAPBRTEMPPLAYER::value_type(dwCharID, pPLAYER));

	ErasePlayerFromPremade(dwCharID);

	if (m_bStatus == BS_PEACE)
		UpdatePlayerQueue();

	return BOWREG_SUCCESS;
}

BYTE CBRSystem::ErasePlayerFromQueue(DWORD dwCharID, DWORD dwKEY)
{
	if (m_bStatus != BS_ALARM)
		return BOWREG_FAIL;

	MAPBRTEMPPLAYER::iterator finder = m_mapBRREG.find(dwCharID);
	if (finder != m_mapBRREG.end())
	{
		if ((*finder).second->m_dwKEY == dwKEY)
		{
			delete (*finder).second;
			m_mapBRREG.erase(finder);
			return BOWREG_SUCCESS;
		}
	}

	return BOWREG_FAIL;
}

void CBRSystem::UpdatePlayerQueue()
{
	if (m_bStatus != BS_PEACE)
		return;

	switch (m_bType)
	{
	case BR_SOLO:
		{
			if ((BYTE) m_mapBRREG.size() >= BR_PARTY_SIZE(m_Mode))
			{
				LPTBRTEAMS pTEAM = new TBRTEAMS();

				for (BYTE i = 0; i < BR_PARTY_SIZE(m_Mode); ++i)
				{
					MAPBRTEMPPLAYER::iterator itFirst = m_mapBRREG.begin();
					if (itFirst == m_mapBRREG.end())
						break;

					LPTBRPLAYERS pPLAYER = new TBRPLAYERS();
					pPLAYER->m_dwCharID = (*itFirst).second->m_dwCharID;
					pPLAYER->m_dwKEY = (*itFirst).second->m_dwKEY;
					pPLAYER->m_bClass = (*itFirst).second->m_bClass;
					pPLAYER->m_strNAME = (*itFirst).second->m_strNAME;

					pTEAM->m_mapBRPlayer.insert(MAPBRPLAYERS::value_type(pPLAYER->m_dwCharID, pPLAYER));

					delete (*itFirst).second;
					m_mapBRREG.erase(itFirst);
				}

				if ((BYTE) pTEAM->m_mapBRPlayer.size() == BR_PARTY_SIZE(m_Mode))
					m_mapBRTeam.insert(MAPBRTEAMS::value_type((BYTE) m_mapBRTeam.size(), pTEAM));
				else
					break;
			}
		}
		break;
	case BR_TEAM:
		{
			for (MAPBRTEAMS::iterator itPremade = m_mapPremadeTeam.begin(); itPremade != m_mapPremadeTeam.end(); ++itPremade)

				if ((BYTE) (*itPremade).second->m_mapBRPlayer.size() != BR_PARTY_SIZE(m_Mode))
				{
					if (!m_mapBRREG.empty())
			{
						return;

					MAPBRTEMPPLAYER::iterator itFirst = m_mapBRREG.begin();
					if (itFirst == m_mapBRREG.end())
						break;

					LPTBRPLAYERS pPLAYER = new TBRPLAYERS();
					pPLAYER->m_dwCharID = (*itFirst).second->m_dwCharID;
					pPLAYER->m_dwKEY = (*itFirst).second->m_dwKEY;
					pPLAYER->m_bClass = (*itFirst).second->m_bClass;
					pPLAYER->m_strNAME = (*itFirst).second->m_strNAME;

					(*itPremade).second->m_mapBRPlayer.insert(MAPBRPLAYERS::value_type(pPLAYER->m_dwCharID, pPLAYER));

					delete (*itFirst).second;
					m_mapBRREG.erase(itFirst);
				}
				
				if ((BYTE) (*itPremade).second->m_mapBRPlayer.size() == BR_PARTY_SIZE(m_Mode))
					m_mapBRTeam.insert(MAPBRTEAMS::value_type((BYTE) m_mapBRTeam.size(), (*itPremade).second));
			}
		}
		break;
	}































}

BYTE CBRSystem::CreateMatch()
{


	BYTE bPlayerCount = (BYTE)m_mapBRREG.size();
	for (MAPBRTEAMS::iterator itPrem = m_mapPremadeTeam.begin(); itPrem != m_mapPremadeTeam.end(); ++itPrem)
	{
		(*itPrem).second->m_bReady = TRUE;
		if (!(*itPrem).second->m_bReady)
		{
			for (MAPBRPLAYERS::iterator itPlayer = (*itPrem).second->m_mapBRPlayer.begin(); itPlayer != (*itPrem).second->m_mapBRPlayer.end(); ++itPlayer)
			{
				_AtlModule.RemoveFromBRTeam((*itPlayer).second->m_dwCharID);
				(*itPrem).second->m_mapBRPlayer.erase(itPlayer);
				delete (*itPlayer).second;
			}
			m_mapPremadeTeam.erase(itPrem);
			delete (*itPrem).second;
		}
		else
		{
			for (MAPBRPLAYERS::iterator itPlayer = (*itPrem).second->m_mapBRPlayer.begin(); itPlayer != (*itPrem).second->m_mapBRPlayer.end(); ++itPlayer)
				bPlayerCount++;
		}
	}

	if (bPlayerCount < m_bMinPlayerCount)
		return FALSE;


	std::vector<LPTBRPLAYERS> mapAttacks, mapSupports, mapEyes;
	MAPBRPLAYERS mapAllChars;

	BYTE bMapSize = (BYTE)m_vBRMap.size();
	BYTE bMapIndex = 0;
	BYTE bHighestVotes = 0;

	BYTE* bVotes = new BYTE[bMapSize];
	for (BYTE i = 0; i < bMapSize; ++i)
		bVotes[i] = 0;

	MAPDWORD::iterator itVotes;
	for (itVotes = m_mapMapVote.begin(); itVotes != m_mapMapVote.end(); ++itVotes)
		bVotes[(*itVotes).second]++;


	if (m_bType == BR_TEAM)
	{
		BYTE Mode[BR_2V2 + 1] = { 0 };
		for (const auto& ModeVote : m_mapVoteMode)
		{
			if (ModeVote.second > BR_2V2)
				continue;

			Mode[ModeVote.second]++;
		}

		m_Mode = Mode[BR_2V2] > Mode[BR_3V3];
	}
	else m_Mode = rand() % (BR_2V2 + 1);

	for (BYTE i = 0; i < bMapSize; ++i)
	{
		if (bVotes[i] > bHighestVotes)
		{
			bHighestVotes = bVotes[i];
			bMapIndex = i;
		}
	}

	if (bMapIndex < m_vBRMap.size())
		m_wMapID = m_vBRMap[bMapIndex].m_wMapID;

	delete[] bVotes;

	if (m_bType == BR_SOLO)
		m_wMapID = m_vBRMap[rand() % m_vBRMap.size()].m_wMapID;

	if (bPlayerCount < 30)
		m_wMapID = DEFAULT_BR_MAP;

	if (m_bType == BR_TEAM)
	{

		for (auto& PremadeTeam : m_mapPremadeTeam)
		{
			if ((BYTE)PremadeTeam.second->m_mapBRPlayer.size() == BR_PARTY_SIZE(m_Mode))
				continue;


			if (m_Mode == BR_2V2 && PremadeTeam.second->m_mapBRPlayer.size() > BR_PARTY_SIZE(m_Mode))
			{
				auto& LastPlayer = PremadeTeam.second->m_mapBRPlayer.rbegin();
				if (LastPlayer != PremadeTeam.second->m_mapBRPlayer.rend())
				{
					auto pREGPLAYER = new BRTEMPPLAYER();
					pREGPLAYER->m_dwCharID = LastPlayer->second->m_dwCharID;
					pREGPLAYER->m_dwKEY = LastPlayer->second->m_dwKEY;
					pREGPLAYER->m_strNAME = LastPlayer->second->m_strNAME;
					pREGPLAYER->m_bClass = LastPlayer->second->m_bClass;

					m_mapBRREG.insert(MAPBRTEMPPLAYER::value_type(LastPlayer->second->m_dwCharID, pREGPLAYER));

					delete (*LastPlayer).second;
					PremadeTeam.second->m_mapBRPlayer.erase(LastPlayer->first);
				}
			}
			else if (PremadeTeam.second->m_mapBRPlayer.size() == BR_PARTY_SIZE(m_Mode) - 1)
			{
				if (!m_mapBRREG.empty())
				{
					auto itTemp = m_mapBRREG.begin();
					if (itTemp == m_mapBRREG.end())
						continue;

					auto pBRPLAYER = new TBRPLAYERS();
					pBRPLAYER->m_dwCharID = (*itTemp).second->m_dwCharID;
					pBRPLAYER->m_dwKEY = (*itTemp).second->m_dwKEY;
					pBRPLAYER->m_strNAME = (*itTemp).second->m_strNAME;
					pBRPLAYER->m_bClass = (*itTemp).second->m_bClass;

					PremadeTeam.second->m_mapBRPlayer.insert(MAPBRPLAYERS::value_type((*itTemp).second->m_dwCharID, pBRPLAYER));

					delete (*itTemp).second;
					m_mapBRREG.erase(itTemp);
				}
			}

		}
	}

	if (m_Mode == BR_3V3)
	{
		for (auto& it = m_mapBRREG.begin(); it != m_mapBRREG.end(); ++it)
		{
			LPTBRPLAYERS pPLAYER = new TBRPLAYERS();
			pPLAYER->m_dwCharID = (*it).second->m_dwCharID;
			pPLAYER->m_dwKEY = (*it).second->m_dwKEY;
			pPLAYER->m_strNAME = (*it).second->m_strNAME;
			pPLAYER->m_bClass = (*it).second->m_bClass;

			switch ((*it).second->m_bClass)
			{

			case TCLASS_WARRIOR:
			case TCLASS_RANGER:
				mapEyes.push_back(pPLAYER);
				break;
			case TCLASS_ARCHER:
			case TCLASS_WIZARD:
				mapAttacks.push_back(pPLAYER);
				break;
			case TCLASS_PRIEST:
			case TCLASS_SORCERER:
				mapSupports.push_back(pPLAYER);
				break;
			default:
				break;
			}

			mapAllChars.insert(MAPBRPLAYERS::value_type((*it).first, pPLAYER));
		}

		random_shuffle(mapEyes.begin(), mapEyes.end());
		random_shuffle(mapAttacks.begin(), mapAttacks.end());
		random_shuffle(mapSupports.begin(), mapSupports.end());

		BYTE bEyeSize = (BYTE)mapEyes.size();
		BYTE bSupportSize = (BYTE)mapSupports.size();
		BYTE bAtcksSize = (BYTE)mapAttacks.size();

		BYTE bCount[3] = { bEyeSize, bSupportSize, bAtcksSize };
		BYTE bMinFairTeams = *std::min_element(bCount, bCount + 3);
		BYTE bCurrentIndex = 0;

		while (bCurrentIndex < bMinFairTeams)
		{
			LPTBRTEAMS pTEAM = new TBRTEAMS();

			if (!mapEyes.empty())
				pTEAM->m_mapBRPlayer.insert(MAPBRPLAYERS::value_type((*mapEyes.begin())->m_dwCharID, (*mapEyes.begin())));

			if (!mapSupports.empty())
				pTEAM->m_mapBRPlayer.insert(MAPBRPLAYERS::value_type((*mapSupports.begin())->m_dwCharID, (*mapSupports.begin())));

			if (!mapAttacks.empty())
				pTEAM->m_mapBRPlayer.insert(MAPBRPLAYERS::value_type((*mapAttacks.begin())->m_dwCharID, (*mapAttacks.begin())));

			if ((BYTE)pTEAM->m_mapBRPlayer.size() == BR_PARTY_SIZE(m_Mode))
			{
				m_mapBRTeam.insert(MAPBRTEAMS::value_type(BYTE(m_mapBRTeam.size()), pTEAM));

				MAPBRTEMPPLAYER::iterator finder = m_mapBRREG.find((*mapEyes.begin())->m_dwCharID);
				if (finder != m_mapBRREG.end())
				{





					delete (*finder).second;
					m_mapBRREG.erase(finder);
				}


				finder = m_mapBRREG.find((*mapSupports.begin())->m_dwCharID);
				if (finder != m_mapBRREG.end())
				{
					delete (*finder).second;
					m_mapBRREG.erase(finder);
				}


				finder = m_mapBRREG.find((*mapAttacks.begin())->m_dwCharID);
				if (finder != m_mapBRREG.end())
				{
					delete (*finder).second;

					m_mapBRREG.erase(finder);
				}

				mapEyes.erase(mapEyes.begin());
				mapSupports.erase(mapSupports.begin());
				mapAttacks.erase(mapAttacks.begin());
			}

			bCurrentIndex++;
		}


		MAPBRPLAYERS mapLeftChars; mapLeftChars.clear();
		for (MAPBRPLAYERS::iterator itAll = mapAllChars.begin(); itAll != mapAllChars.end(); ++itAll)






		{
			for (const auto& Support : mapSupports)
			{
				if (Support->m_dwCharID == (*itAll).first)
				{
					mapLeftChars.insert(MAPBRPLAYERS::value_type((*itAll).first, (*itAll).second));
					MAPBRTEMPPLAYER::iterator finder = m_mapBRREG.find((*itAll).second->m_dwCharID);
					if (finder != m_mapBRREG.end())
					{
						delete (*finder).second;
						m_mapBRREG.erase(finder);
					}

					break;
				}
			}





			for (const auto& Attack : mapAttacks)
			{
				if (Attack->m_dwCharID == (*itAll).first)
				{
					mapLeftChars.insert(MAPBRPLAYERS::value_type((*itAll).first, (*itAll).second));







					MAPBRTEMPPLAYER::iterator finder = m_mapBRREG.find((*itAll).second->m_dwCharID);
					if (finder != m_mapBRREG.end())
					{
						delete (*finder).second;
						m_mapBRREG.erase(finder);
					}

					break;
				}
			}

			for (const auto& Eye : mapEyes)
			{
				if (Eye->m_dwCharID == (*itAll).first)
				{
					mapLeftChars.insert(MAPBRPLAYERS::value_type((*itAll).first, (*itAll).second));


					MAPBRTEMPPLAYER::iterator finder = m_mapBRREG.find((*itAll).second->m_dwCharID);
					if (finder != m_mapBRREG.end())
					{
						delete (*finder).second;
						m_mapBRREG.erase(finder);
					}

					break;
				}
			}
		}

		vector< pair < DWORD, LPTBRPLAYERS > > vLeftChar;
		for (MAPBRPLAYERS::iterator itLeftP = mapLeftChars.begin(); itLeftP != mapLeftChars.end(); ++itLeftP)
			vLeftChar.push_back(make_pair((*itLeftP).first, (*itLeftP).second));
		random_shuffle(vLeftChar.begin(), vLeftChar.end());

		while ((BYTE)vLeftChar.size() >= BR_PARTY_SIZE(m_Mode))
		{
			LPTBRTEAMS pTEAM = new TBRTEAMS();

			for (BYTE i = 0; i < BR_PARTY_SIZE(m_Mode); ++i)
			{
				pTEAM->m_mapBRPlayer.insert(MAPBRPLAYERS::value_type((*vLeftChar.begin()).first, (*vLeftChar.begin()).second));
				vLeftChar.erase(vLeftChar.begin());
			}

			if (pTEAM->m_mapBRPlayer.size() == BR_PARTY_SIZE(m_Mode))
				m_mapBRTeam.insert(MAPBRTEAMS::value_type((BYTE)m_mapBRTeam.size(), pTEAM));
			else
				break;
		}

		for (vector< pair < DWORD, LPTBRPLAYERS > >::iterator itPointer = vLeftChar.begin(); itPointer != vLeftChar.end(); ++itPointer)
			delete (*itPointer).second; //Delete the pointer of the players, but the pointer will stay in the reg.
	}
	else if (m_Mode == BR_2V2)
	{
		for (auto& it = m_mapBRREG.begin(); it != m_mapBRREG.end(); ++it)
		{
			LPTBRPLAYERS pPLAYER = new TBRPLAYERS();
			pPLAYER->m_dwCharID = (*it).second->m_dwCharID;
			pPLAYER->m_dwKEY = (*it).second->m_dwKEY;
			pPLAYER->m_strNAME = (*it).second->m_strNAME;
			pPLAYER->m_bClass = (*it).second->m_bClass;

			switch ((*it).second->m_bClass)
			{
			case TCLASS_ARCHER:
			case TCLASS_WIZARD:
			case TCLASS_RANGER:
				mapAttacks.push_back(pPLAYER);
				break;
			case TCLASS_WARRIOR:
			case TCLASS_PRIEST:
			case TCLASS_SORCERER:
				mapSupports.push_back(pPLAYER);
				break;
			default:
				break;
			}




			mapAllChars.insert(MAPBRPLAYERS::value_type((*it).first, pPLAYER));
		}




		random_shuffle(mapAttacks.begin(), mapAttacks.end());
		random_shuffle(mapSupports.begin(), mapSupports.end());




		BYTE bSupportSize = (BYTE)mapSupports.size();
		BYTE bAtcksSize = (BYTE)mapAttacks.size();



		BYTE bCount[2] = { bSupportSize, bAtcksSize };
		BYTE bMinFairTeams = *std::min_element(bCount, bCount + 2);
		BYTE bCurrentIndex = 0;



		while (bCurrentIndex < bMinFairTeams)
		{
			LPTBRTEAMS pTEAM = new TBRTEAMS();

			if (!mapSupports.empty())
				pTEAM->m_mapBRPlayer.insert(MAPBRPLAYERS::value_type((*mapSupports.begin())->m_dwCharID, (*mapSupports.begin())));

			if (!mapAttacks.empty())
				pTEAM->m_mapBRPlayer.insert(MAPBRPLAYERS::value_type((*mapAttacks.begin())->m_dwCharID, (*mapAttacks.begin())));

			if ((BYTE)pTEAM->m_mapBRPlayer.size() == BR_PARTY_SIZE(m_Mode))
			{
				m_mapBRTeam.insert(MAPBRTEAMS::value_type(BYTE(m_mapBRTeam.size()), pTEAM));

				auto finder = m_mapBRREG.find((*mapSupports.begin())->m_dwCharID);
				if (finder != m_mapBRREG.end())
				{
					delete (*finder).second;
					m_mapBRREG.erase(finder);
				}

				finder = m_mapBRREG.find((*mapAttacks.begin())->m_dwCharID);
				if (finder != m_mapBRREG.end())
				{
					delete (*finder).second;
					m_mapBRREG.erase(finder);
				}

				mapSupports.erase(mapSupports.begin());
				mapAttacks.erase(mapAttacks.begin());
			}

			bCurrentIndex++;
		}

		MAPBRPLAYERS mapLeftChars; mapLeftChars.clear();
		for (MAPBRPLAYERS::iterator itAll = mapAllChars.begin(); itAll != mapAllChars.end(); ++itAll)
		{

			for (const auto& Support : mapSupports)
			{
				if (Support->m_dwCharID == (*itAll).first)
				{
					mapLeftChars.insert(MAPBRPLAYERS::value_type((*itAll).first, (*itAll).second));
					MAPBRTEMPPLAYER::iterator finder = m_mapBRREG.find((*itAll).second->m_dwCharID);
					if (finder != m_mapBRREG.end())
					{
						delete (*finder).second;
						m_mapBRREG.erase(finder);
					}



					break;
				}
			}

			for (const auto& Attack : mapAttacks)
			{
				if (Attack->m_dwCharID == (*itAll).first)
				{
					mapLeftChars.insert(MAPBRPLAYERS::value_type((*itAll).first, (*itAll).second));
					MAPBRTEMPPLAYER::iterator finder = m_mapBRREG.find((*itAll).second->m_dwCharID);
					if (finder != m_mapBRREG.end())
					{
						delete (*finder).second;
						m_mapBRREG.erase(finder);
					}

					break;
				}
			}
		}



		vector< pair < DWORD, LPTBRPLAYERS > > vLeftChar;
		for (MAPBRPLAYERS::iterator itLeftP = mapLeftChars.begin(); itLeftP != mapLeftChars.end(); ++itLeftP)
			vLeftChar.push_back(make_pair((*itLeftP).first, (*itLeftP).second));
		random_shuffle(vLeftChar.begin(), vLeftChar.end());

		while ((BYTE)vLeftChar.size() >= BR_PARTY_SIZE(m_Mode))
		{
			LPTBRTEAMS pTEAM = new TBRTEAMS();

			for (BYTE i = 0; i < BR_PARTY_SIZE(m_Mode); ++i)
			{


				pTEAM->m_mapBRPlayer.insert(MAPBRPLAYERS::value_type((*vLeftChar.begin()).first, (*vLeftChar.begin()).second));
				vLeftChar.erase(vLeftChar.begin());
			}



			if (pTEAM->m_mapBRPlayer.size() == BR_PARTY_SIZE(m_Mode))
				m_mapBRTeam.insert(MAPBRTEAMS::value_type((BYTE)m_mapBRTeam.size(), pTEAM));
			else
				break;
		}

		for (vector< pair < DWORD, LPTBRPLAYERS > >::iterator itPointer = vLeftChar.begin(); itPointer != vLeftChar.end(); ++itPointer)
			delete (*itPointer).second; //Delete the pointer of the players, but the pointer will stay in the reg.
	}

	if (m_bType == BR_TEAM)
	{
		for (MAPBRTEAMS::iterator itPremade = m_mapPremadeTeam.begin(); itPremade != m_mapPremadeTeam.end(); ++itPremade)
		{
			if ((BYTE)(*itPremade).second->m_mapBRPlayer.size() == BR_PARTY_SIZE(m_Mode))
			{

				LPTBRTEAMS pTEAM = new TBRTEAMS();

				for (MAPBRPLAYERS::iterator itPremadePlayer = (*itPremade).second->m_mapBRPlayer.begin(); itPremadePlayer != (*itPremade).second->m_mapBRPlayer.end(); ++itPremadePlayer)
				{
					LPTBRPLAYERS pPLAYER = new TBRPLAYERS();
					pPLAYER->m_dwCharID = (*itPremadePlayer).second->m_dwCharID;
					pPLAYER->m_dwKEY = (*itPremadePlayer).second->m_dwKEY;
					pPLAYER->m_strNAME = (*itPremadePlayer).second->m_strNAME;
					pPLAYER->m_bClass = (*itPremadePlayer).second->m_bClass;

					pTEAM->m_mapBRPlayer.insert(MAPBRPLAYERS::value_type(pPLAYER->m_dwCharID, pPLAYER));

					delete (*itPremadePlayer).second;
					(*itPremade).second->m_mapBRPlayer.erase(itPremadePlayer);
				}

				Log("Inserting PREMADE");
				m_mapBRTeam.insert(MAPBRTEAMS::value_type((BYTE)m_mapBRTeam.size(), pTEAM));

				delete (*itPremade).second;
				m_mapPremadeTeam.erase(itPremade);
			}
			else
			{
				CString dbug;
				dbug.Format("PlayerSize : %d, BR_PARTY: %d", (BYTE)(*itPremade).second->m_mapBRPlayer.size(), BR_PARTY_SIZE(m_Mode));
				Log(dbug);
			}
		}
	}




	for (MAPBRTEAMS::iterator it = m_mapBRTeam.begin(); it != m_mapBRTeam.end(); ++it)
	{
		for (MAPBRPLAYERS::iterator jirk = (*it).second->m_mapBRPlayer.begin(); jirk != (*it).second->m_mapBRPlayer.end(); ++jirk)
		{
			CString strLog;
			strLog.Format("Mode : %s, Teams : %d Players : %d Current Team : %d Current Player : %d", m_Mode == BR_3V3 ? "3" : "2", m_mapBRTeam.size(), (*it).second->m_mapBRPlayer.size(), (*it).first, (*jirk).first);
			Log(strLog);
		}
	}


	for (MAPBRTEMPPLAYER::iterator itReg = m_mapBRREG.begin(); itReg != m_mapBRREG.end(); ++itReg) //For later implementations of after reg
	{
		if ((*itReg).second)
			delete (*itReg).second;
	}
	m_mapBRREG.clear();

	_AtlModule.TeleportBRTeam(m_mapBRTeam, TRUE);

	/*


	MAPBRPLAYERS mapLeftChars; mapLeftChars.clear();
	for (MAPBRPLAYERS::iterator itAll = mapAllChars.begin(); itAll != mapAllChars.end(); ++itAll)
	{
		if (mapSupports.find((*itAll).first) != mapSupports.end() ||
			mapEyes.find((*itAll).first) != mapEyes.end() ||
			mapAttacks.find((*itAll).first) != mapAttacks.end())



		{
			mapLeftChars.insert(MAPBRPLAYERS::value_type((*itAll).first, (*itAll).second));
			MAPBRTEMPPLAYER::iterator finder = m_mapBRREG.find((*itAll).second->m_dwCharID);
			if (finder != m_mapBRREG.end())
			{
				delete (*finder).second;
				m_mapBRREG.erase(finder);
			}
		}
	}

	vector< pair < DWORD, LPTBRPLAYERS > > vLeftChar;
	for (MAPBRPLAYERS::iterator itLeftP = mapLeftChars.begin(); itLeftP != mapLeftChars.end(); ++itLeftP)
		vLeftChar.push_back(make_pair((*itLeftP).first, (*itLeftP).second));
	random_shuffle(vLeftChar.begin(), vLeftChar.end());

	while ((BYTE) vLeftChar.size() >= BR_PARTY_SIZE(m_Mode))
	{
		LPTBRTEAMS pTEAM = new TBRTEAMS();

		for (BYTE i = 0; i < BR_PARTY_SIZE(m_Mode); ++i)
		{
			pTEAM->m_mapBRPlayer.insert(MAPBRPLAYERS::value_type((*vLeftChar.begin()).first, (*vLeftChar.begin()).second));
			vLeftChar.erase(vLeftChar.begin());
		}

		if (pTEAM->m_mapBRPlayer.size() == BR_PARTY_SIZE(m_Mode))
			m_mapBRTeam.insert(MAPBRTEAMS::value_type((BYTE) m_mapBRTeam.size(), pTEAM));
		else
			break;
	}

	for (vector< pair < DWORD, LPTBRPLAYERS > >::iterator itPointer = vLeftChar.begin(); itPointer != vLeftChar.end(); ++itPointer)
		delete (*itPointer).second; //Delete the pointer of the players, but the pointer will stay in the reg.

	if (m_bType == BR_TEAM)
	{
		for (MAPBRTEAMS::iterator itPremade = m_mapPremadeTeam.begin(); itPremade != m_mapPremadeTeam.end(); ++itPremade)
		{
			if ((BYTE) (*itPremade).second->m_mapBRPlayer.size() == BR_PARTY_SIZE(m_Mode))
			{
				LPTBRTEAMS pTEAM = new TBRTEAMS();

				for (MAPBRPLAYERS::iterator itPremadePlayer = (*itPremade).second->m_mapBRPlayer.begin(); itPremadePlayer != (*itPremade).second->m_mapBRPlayer.end(); ++itPremadePlayer)
				{
					LPTBRPLAYERS pPLAYER = new TBRPLAYERS();
					pPLAYER->m_dwCharID = (*itPremadePlayer).second->m_dwCharID;
					pPLAYER->m_dwKEY = (*itPremadePlayer).second->m_dwKEY;
					pPLAYER->m_strNAME = (*itPremadePlayer).second->m_strNAME;
					pPLAYER->m_bClass = (*itPremadePlayer).second->m_bClass;

					pTEAM->m_mapBRPlayer.insert(MAPBRPLAYERS::value_type(pPLAYER->m_dwCharID, pPLAYER));

					delete (*itPremadePlayer).second;
					(*itPremade).second->m_mapBRPlayer.erase(itPremadePlayer);
				}

				m_mapBRTeam.insert(MAPBRTEAMS::value_type((BYTE) m_mapBRTeam.size(), pTEAM));

				delete (*itPremade).second;
				m_mapPremadeTeam.erase(itPremade);
			}
		}
	}

	for(MAPBRTEAMS::iterator it = m_mapBRTeam.begin(); it != m_mapBRTeam.end(); ++it)
	{
		for(MAPBRPLAYERS::iterator jirk = (*it).second->m_mapBRPlayer.begin(); jirk != (*it).second->m_mapBRPlayer.end(); ++jirk )
		{
			CString strLog;
			strLog.Format("Teams : %d Players : %d Current Team : %d Current Player : %d", m_mapBRTeam.size(), (*it).second->m_mapBRPlayer.size(), (*it).first, (*jirk).first);
			Log(strLog);
		}
	}

	for(MAPBRTEMPPLAYER::iterator itReg = m_mapBRREG.begin(); itReg != m_mapBRREG.end(); ++itReg) //For later implementations of after reg
	{
		if ((*itReg).second)
			delete (*itReg).second;
	}
	m_mapBRREG.clear();

	_AtlModule.TeleportBRTeam(m_mapBRTeam, TRUE);
}





	*/
	return TRUE;
}

void CBRSystem::Release()
{
	m_bRunning = FALSE;
	m_bStatus = BS_NORMAL;
	m_dwTick = 0;
	m_dwPeaceTick = 0;
	m_Mode = BR_3V3;

	for (MAPBRTEAMS::iterator itTeam = m_mapBRTeam.begin(); itTeam != m_mapBRTeam.end(); ++itTeam)
	{
		for (MAPBRPLAYERS::iterator itPlayer = (*itTeam).second->m_mapBRPlayer.begin(); itPlayer != (*itTeam).second->m_mapBRPlayer.end(); ++itPlayer)
			delete (*itPlayer).second;
		(*itTeam).second->m_mapBRPlayer.clear();

		delete (*itTeam).second;
	}

	for (MAPBRTEAMS::iterator itPremade = m_mapPremadeTeam.begin(); itPremade != m_mapPremadeTeam.end(); ++itPremade)
	{
		for (MAPBRPLAYERS::iterator itPlayer = (*itPremade).second->m_mapBRPlayer.begin(); itPlayer != (*itPremade).second->m_mapBRPlayer.end(); ++itPlayer)
			delete (*itPlayer).second;
		(*itPremade).second->m_mapBRPlayer.clear();

		delete (*itPremade).second;
	}
	m_mapPremadeTeam.clear();

	for (MAPBRTEMPPLAYER::iterator itReg = m_mapBRREG.begin(); itReg != m_mapBRREG.end(); ++itReg)
		delete (*itReg).second;
	m_mapBRREG.clear();

	m_mapBRTeam.clear();
	m_mapMapVote.clear();
	m_mapVoteMode.clear();
}

void CBRSystem::SwitchType()
{
	m_bType = !m_bType;
}

void CBRSystem::JoinPremadeTeam(DWORD dwChiefID, 
								DWORD dwChiefKEY, 
								BYTE bChiefClass, 
								CString strChiefName, 
								DWORD dwMateID, 
								DWORD dwMateKEY, 
								BYTE bMateClass, 
								CString strMateName)
{
	if (m_bType != BR_TEAM)
		return;

	MAPBRTEAMS::iterator it;
	MAPBRPLAYERS::iterator itP;


	it = m_mapPremadeTeam.find(dwChiefID);
	if (it == m_mapPremadeTeam.end()) 
	{
		if (FindPlayerInPremade(dwChiefID) || FindPlayerInPremade(dwMateID))
			return;

		LPTBRTEAMS pTEAM = new TBRTEAMS();

		LPTBRPLAYERS pCHIEF = new TBRPLAYERS();
		pCHIEF->m_dwCharID = dwChiefID;
		pCHIEF->m_dwKEY = dwChiefKEY;
		pCHIEF->m_strNAME = strChiefName;
		pCHIEF->m_bClass = bChiefClass;
		pCHIEF->m_bReady = TRUE;
		pTEAM->m_mapBRPlayer.insert(MAPBRPLAYERS::value_type(dwChiefID, pCHIEF));
		
		LPTBRPLAYERS pPLAYER = new TBRPLAYERS();
		pPLAYER->m_dwCharID = dwMateID;
		pPLAYER->m_dwKEY = dwMateKEY;
		pPLAYER->m_strNAME = strMateName;
		pPLAYER->m_bClass = bMateClass;
		pPLAYER->m_bReady = FALSE;
		pTEAM->m_mapBRPlayer.insert(MAPBRPLAYERS::value_type(dwMateID, pPLAYER));

		m_mapPremadeTeam.insert(MAPBRTEAMS::value_type(dwChiefID, pTEAM));
	}
	else
	{
		if (FindPlayerInPremade(dwMateID) || (*it).second->m_mapBRPlayer.size() >= 3)
			return;

		LPTBRPLAYERS pPLAYER = new TBRPLAYERS();
		pPLAYER->m_dwCharID = dwMateID;
		pPLAYER->m_dwKEY = dwMateKEY;
		pPLAYER->m_strNAME = strMateName;
		pPLAYER->m_bClass = bMateClass;
		pPLAYER->m_bReady = FALSE;

		(*it).second->m_mapBRPlayer.insert(MAPBRPLAYERS::value_type(dwMateID, pPLAYER));
	}

	ErasePlayerFromQueue(dwChiefID, dwChiefKEY);
	ErasePlayerFromQueue(dwMateID, dwMateKEY);

	_AtlModule.UpdateBRTeam(dwChiefID);
}

BOOL CBRSystem::FindPlayerInPremade(DWORD dwCharID)
{
	MAPBRTEAMS::iterator it;
	MAPBRPLAYERS::iterator itP;
	for (it = m_mapPremadeTeam.begin(); it != m_mapPremadeTeam.end(); ++it)
	{
		for (itP = (*it).second->m_mapBRPlayer.begin(); itP != (*it).second->m_mapBRPlayer.end(); ++itP)
		{
			if ((*itP).second->m_dwCharID == dwCharID)
				return TRUE;
		}
	}

	return FALSE;
}

BOOL CBRSystem::FindPlayerInTeam(DWORD dwCharID)
{
	MAPBRTEAMS::iterator it;
	MAPBRPLAYERS::iterator itP;
	for (it = m_mapBRTeam.begin(); it != m_mapBRTeam.end(); ++it)
	{
		for (itP = (*it).second->m_mapBRPlayer.begin(); itP != (*it).second->m_mapBRPlayer.end(); ++itP)
		{
			if ((*itP).second->m_dwCharID == dwCharID)
				return TRUE;
		}
	}

	return FALSE;
}

LPTBRPLAYERS CBRSystem::FindPlayerPointerInTeam(DWORD dwCharID)
{
	MAPBRTEAMS::iterator it;
	MAPBRPLAYERS::iterator itP;
	for (it = m_mapBRTeam.begin(); it != m_mapBRTeam.end(); ++it)
	{
		for (itP = (*it).second->m_mapBRPlayer.begin(); itP != (*it).second->m_mapBRPlayer.end(); ++itP)
		{
			if ((*itP).second->m_dwCharID == dwCharID)
				return (*itP).second;
		}
	}

	return NULL;	
}

BYTE CBRSystem::GetPremadePlayerCountByChief(DWORD dwChiefID)
{
	MAPBRTEAMS::iterator finder = m_mapPremadeTeam.find(dwChiefID);
	if (finder != m_mapPremadeTeam.end())
		return (BYTE) (*finder).second->m_mapBRPlayer.size();

	return 0;
}

DWORD CBRSystem::GetChiefIDByMateID(DWORD dwCharID)
{
	for (MAPBRTEAMS::iterator it = m_mapPremadeTeam.begin(); it != m_mapPremadeTeam.end(); ++it)
	{
		for (MAPBRPLAYERS::iterator itPlayer = (*it).second->m_mapBRPlayer.begin(); itPlayer != (*it).second->m_mapBRPlayer.end(); ++itPlayer)
		{
			if ((*itPlayer).second->m_dwCharID == dwCharID)
				return (*it).first;
		}
	}

	return 0;
}

void CBRSystem::ErasePlayerFromPremade(DWORD dwCharID)
{
	try
	{
	if (!FindPlayerInPremade(dwCharID) || m_bType != BR_TEAM)
		return;

	MAPBRTEAMS::iterator finder = m_mapPremadeTeam.find(dwCharID);
	if (finder != m_mapPremadeTeam.end())
	{
		if ((BYTE) (*finder).second->m_mapBRPlayer.size() < BR_TEAMMATE_MAX_COUNT(BR_3V3))
		{
			for (MAPBRPLAYERS::iterator it = (*finder).second->m_mapBRPlayer.begin(); it != (*finder).second->m_mapBRPlayer.end(); ++it)
			{
				_AtlModule.RemoveFromBRTeam((*it).second->m_dwCharID);
				delete (*it).second;
			}
			(*finder).second->m_mapBRPlayer.clear();

			delete (*finder).second;
			m_mapPremadeTeam.erase(finder);
		}
		else
		{
			MAPBRPLAYERS::iterator itNewChief;
			for (MAPBRPLAYERS::iterator it = (*finder).second->m_mapBRPlayer.begin(); it != (*finder).second->m_mapBRPlayer.end(); ++it)
			{
				if ((*it).second->m_dwCharID == dwCharID)
				{
					_AtlModule.RemoveFromBRTeam(dwCharID);
					delete (*it).second;
					(*finder).second->m_mapBRPlayer.erase(it);
					break;
				}
			}

			itNewChief = (*finder).second->m_mapBRPlayer.begin();
			if (itNewChief == (*finder).second->m_mapBRPlayer.end())
				return;

			DWORD dwNewChief = (*itNewChief).first;

			LPTBRTEAMS pNEWTEAM = new TBRTEAMS();
			for (MAPBRPLAYERS::iterator itOldPlayer = (*finder).second->m_mapBRPlayer.begin(); itOldPlayer != (*finder).second->m_mapBRPlayer.end(); ++itOldPlayer)
			{
				LPTBRPLAYERS pNEWPLAYER = new TBRPLAYERS();
				pNEWPLAYER->m_dwCharID = (*itOldPlayer).second->m_dwCharID;
				pNEWPLAYER->m_dwKEY = (*itOldPlayer).second->m_dwKEY;
				pNEWPLAYER->m_strNAME = (*itOldPlayer).second->m_strNAME;
				pNEWPLAYER->m_bClass = (*itOldPlayer).second->m_bClass;
				
				pNEWTEAM->m_mapBRPlayer.insert(MAPBRPLAYERS::value_type(pNEWPLAYER->m_dwCharID, pNEWPLAYER));
				delete (*itOldPlayer).second;
			}
			
			(*finder).second->m_mapBRPlayer.clear();
			delete (*finder).second;
			m_mapPremadeTeam.erase(finder);
			m_mapPremadeTeam.insert(MAPBRTEAMS::value_type(dwNewChief, pNEWTEAM));

			_AtlModule.UpdateBRTeam(dwNewChief);
		}
	}
	else
	{
		MAPBRTEAMS::iterator itTeam, itTeamBuff;
		MAPBRPLAYERS::iterator itP, itPBuff;
		for (itTeam = m_mapPremadeTeam.begin(); itTeam != m_mapPremadeTeam.end(); ++itTeam)
		{
			for (itP = (*itTeam).second->m_mapBRPlayer.begin(); itP != (*itTeam).second->m_mapBRPlayer.end(); ++itP)
				if ((*itP).second->m_dwCharID == dwCharID)
				{
					itPBuff = itP;
					itTeamBuff = itTeam;
					break;
				}
		}

		itP = itPBuff;
		itTeam = itTeamBuff;
		if (itTeam != m_mapPremadeTeam.end() && itP != (*itTeam).second->m_mapBRPlayer.end())
		{
			if ((BYTE) (*itTeam).second->m_mapBRPlayer.size() < BR_TEAMMATE_MAX_COUNT(BR_3V3))
			{
				for (itP = (*itTeam).second->m_mapBRPlayer.begin(); itP != (*itTeam).second->m_mapBRPlayer.end(); ++itP)
				{
					_AtlModule.RemoveFromBRTeam((*itP).second->m_dwCharID);
					delete (*itP).second;
				}
				(*itTeam).second->m_mapBRPlayer.clear();

				delete (*itTeam).second;
				m_mapPremadeTeam.erase(itTeam);
			}
			else
			{
				if ((*itP).second->m_dwCharID == dwCharID)
				{
					_AtlModule.RemoveFromBRTeam(dwCharID);
					delete (*itP).second;
					(*itTeam).second->m_mapBRPlayer.erase(itP);
				}
				else
				{
					for (itP = (*itTeam).second->m_mapBRPlayer.begin(); itP != (*itTeam).second->m_mapBRPlayer.end(); ++itP)
					{
						if ((*itP).second->m_dwCharID == dwCharID)
						{
							_AtlModule.RemoveFromBRTeam(dwCharID);
							delete (*itP).second;
							(*itTeam).second->m_mapBRPlayer.erase(itP);
							break;
						}
					}
				}
			}

			_AtlModule.UpdateBRTeam((*itTeam).first);
		}
	}
	}
	catch(...)
	{
		Log("Error at ErasePlayerFromPremade.");
	}
}

void CBRSystem::VoteForMap(DWORD dwUserID, CString strMap)
{
	for (BYTE i = 0; i < m_vBRMap.size(); ++i)
	{
		if (m_vBRMap[i].m_strName == strMap)
		{
			if (m_mapMapVote.find(dwUserID) != m_mapMapVote.end())
				return;

			m_mapMapVote.insert(MAPDWORD::value_type(dwUserID, i));
			break;
		}
	}
}

void CBRSystem::VoteForMode(DWORD dwUserID, BYTE Mode)
{
	if (m_mapVoteMode.find(dwUserID) != m_mapVoteMode.end())
		return;

	m_mapVoteMode.insert(std::make_pair(dwUserID, Mode));
}

BYTE CBRSystem::FlagPlayerReady(DWORD dwCharID, DWORD dwKEY)
{
	for (MAPBRTEAMS::iterator itTeam = m_mapPremadeTeam.begin(); itTeam != m_mapPremadeTeam.end(); ++itTeam)
	{
		for (MAPBRPLAYERS::iterator itPlayer = (*itTeam).second->m_mapBRPlayer.begin(); itPlayer != (*itTeam).second->m_mapBRPlayer.end(); ++itPlayer)
		{
			if ((*itPlayer).second->m_dwCharID == dwCharID)
			{
				(*itPlayer).second->m_bReady = TRUE;
				return TRUE;
			}
		}
	}

	return FALSE;
}

BYTE CBRSystem::FlagTeamReady(DWORD dwChiefID)
{
	MAPBRTEAMS::iterator itTeam = m_mapPremadeTeam.find(dwChiefID);
	if (itTeam == m_mapPremadeTeam.end())
		return FALSE;

	for (MAPBRPLAYERS::iterator itPlayer = (*itTeam).second->m_mapBRPlayer.begin(); itPlayer != (*itTeam).second->m_mapBRPlayer.end(); ++itPlayer)
	{
		if ((*itPlayer).second->m_dwCharID == dwChiefID)
			continue;

		if (!(*itPlayer).second->m_bReady)
			return FALSE;
	}

	(*itTeam).second->m_bReady = TRUE;

	return TRUE;
}

void CBRSystem::AddMap(WORD wMapID, CString strName)
{
	BRMAP Map;
	Map.m_wMapID = wMapID;
	Map.m_strName = strName;

	m_vBRMap.push_back(Map);
}

void CBRSystem::OnEnd()
{
	if (!m_pBRServer)
		return;

	_AtlModule.TeleportBRTeam(m_mapBRTeam, FALSE);

	SetNextTime();
	Release();
}

void CBRSystem::ReleaseSinglePlayer(DWORD dwCharID, DWORD dwKEY)
{
	LPTBRPLAYERS Player = FindPlayerPointerInTeam(dwCharID);
	if (Player && Player->m_dwKEY == dwKEY)
		_AtlModule.TeleportBRPlayer(dwCharID, dwKEY);
}

/*============ LOGS ============*/
void CBRSystem::Log(CString strLog)
{
	FILE* pLog = fopen("C:\\TServices_GSP\\BRDebug.log", "a+");
	if(!pLog)
		pLog = fopen("C:\\TServices_GSP\\BRDebug.log", "w+");
	if (!pLog)
		return;

	CString strText;
	CTime tTime(CTime::GetCurrentTime());
	strText.Format("[BR-WORLD] %s - %s\n", tTime.Format("%c"), strLog);

	fprintf(pLog, strText);
	fclose(pLog);
}