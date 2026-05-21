#include "StdAfx.h"
#include <SvrInc.h>
#include "TMapSvrModule.h"

CTCompanion::CTCompanion(void)
{
	m_dwMonID = 0;
	m_bCompanionSlot = 0;
	m_bEffect = 0;
	m_bType = OT_COMPANION;
}

CTCompanion::~CTCompanion(void)
{
	//_AtlModule.ReleaseSelfMonID(m_dwID);
}

void CTCompanion::OnDie( DWORD dwAttackID, BYTE bObjectType, WORD wTempMonID ) 
{
	if(!m_pMON)
		return;
	CTPlayer *pCreater = FindHost(m_dwHostID);

	if( m_pMAP && pCreater )
	{
		VTMONSTER vMONS;
		vMONS.clear();

		m_pMAP->GetNeighbor(
			&vMONS,
			m_fPosX,
			m_fPosZ);

		while(!vMONS.empty())
		{
			CTMonster *pAGGROMON = vMONS.back();

			MAPTAGGRO::iterator it = pAGGROMON->m_mapAggro.find(MAKEINT64( m_dwID, m_bType));
			if( it != pAGGROMON->m_mapAggro.end() )
			{
				pAGGROMON->AddAggro(m_dwHostID, m_dwHostID, OT_PC, pCreater->GetWarCountry(), (*it).second.m_dwAggro);
				pAGGROMON->LeaveAggro(m_dwHostID, m_dwID, m_bType);
			}

			vMONS.pop_back();
		}

		vMONS.clear();
	}
}

void CTCompanion::Recover(DWORD dwTick)
{
}
