#include "StdAfx.h"


CTInven::CTInven()
{
	m_mapTITEM.clear();
	m_pTITEM = NULL;

	m_bInvenID = 0;
	m_wItemID = 0;
	m_dEndTime = 0;
	m_bELD = 0;
}

CTInven::~CTInven()
{
	MAPTITEM::iterator itTITEM;

	for( itTITEM = m_mapTITEM.begin(); itTITEM != m_mapTITEM.end(); itTITEM++)
		delete (*itTITEM).second;

	m_mapTITEM.clear();
}

CTItem *CTInven::FindTItem( BYTE bItemID)
{
	MAPTITEM::iterator finder = m_mapTITEM.find(bItemID);

	if( finder != m_mapTITEM.end() )
		return (*finder).second;

	return NULL;
}

BYTE CTInven::GetEasePos( CTItem *pTItem)
{
	MAPTITEM::iterator itTITEM;

	for( itTITEM = m_mapTITEM.begin(); itTITEM != m_mapTITEM.end(); itTITEM++)
		if( (*(*itTITEM).second) == (*pTItem) &&
			(*itTITEM).second->m_bCount < (*itTITEM).second->m_pTITEM->m_bStack )
			return (*itTITEM).second->m_bItemID;

	return INVALID_SLOT;
}

BYTE CTInven::GetBlankPos()
{
	for( BYTE i=0; i<m_pTITEM->m_bSlotCount; i++)
		if( m_mapTITEM.find(i) == m_mapTITEM.end() )
			return i;

	return INVALID_SLOT;
}

void CTInven::Copy(CTInven* pInven)
{
	m_bELD = pInven->m_bELD;
	m_bInvenID = pInven->m_bInvenID;
	m_dEndTime = pInven->m_dEndTime;
	m_pTITEM = pInven->m_pTITEM;
	m_wItemID = pInven->m_wItemID;

	for(MAPTITEM::iterator itItem = pInven->m_mapTITEM.begin(); itItem != pInven->m_mapTITEM.end(); ++itItem)
	{
		CTItem* pItem = new CTItem();
		pItem->Copy((*itItem).second, TRUE);

		m_mapTITEM.insert(MAPTITEM::value_type((*itItem).first, pItem));
	}
}