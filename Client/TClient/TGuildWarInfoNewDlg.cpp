#include "StdAfx.h"
#include "TFameRank.h"
#include "TGuildWarInfoNewDlg.h"
#include "TClientGame.h"
#include "Resource.h"
#include "TClient.h"
BYTE CTGuildWarInfoNewDlg::m_bTabIndex = TGUILD_WARINFO;


CTGuildWarInfoNewDlg::CTGuildWarInfoNewDlg(TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc)
	: ITInnerFrame(pParent, pDesc, TGUILD_WARINFO)
{

	m_pList = static_cast<TList*>(FindKid(ID_CTRLINST_LIST));
	m_pList2 = static_cast<TList*>(FindKid(27655));
	m_pList3 = static_cast<TList*>(FindKid(28163));

}

CTGuildWarInfoNewDlg::~CTGuildWarInfoNewDlg()
{

}

void CTGuildWarInfoNewDlg::SetCurMode()
{
	m_pList->RemoveAll();
	m_pList2->RemoveAll();
	m_pList3->RemoveAll();

	TComponent * m_pListScr = m_pList->FindKid(49595);
	TComponent * m_pList2Scr = m_pList2->FindKid(49600);
	TComponent * m_pList3Scr = m_pList3->FindKid(50062);

	if (m_pList->GetItemCount() <= 21)
		m_pListScr->ShowComponent(FALSE);
	else
		m_pListScr->ShowComponent(TRUE);

	if (m_pList2->GetItemCount() <= 9)
		m_pList2Scr->ShowComponent(FALSE);
	else
		m_pList2Scr->ShowComponent(TRUE);

	if (m_pList3->GetItemCount() <= 7)
		m_pList3Scr->ShowComponent(FALSE);
	else
		m_pList3Scr->ShowComponent(TRUE);

	CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	INT nCNT = (INT)pCmd->GetCountTerritory();
	for (INT i = 0; i<nCNT; ++i)
	{
		const Territory& info = pCmd->GetTerritory(i);

		DWORD color = 0;
		switch (info.m_wCastleID)
		{
		case 4: color = COLOR_CASTLE_HESED; break;
		case 8: color = COLOR_CASTLE_ARDRI; break;
		case 12: color = COLOR_CASTLE_GEHBRA; break;
		case 16: color = COLOR_CASTLE_TYCON; break;
		case 20: color = COLOR_CASTLE_WOODLAND; break;
		default: color = COLOR_CASTLE_NONE;
		}

		m_pList->ApplyUserColor(TRUE);
		m_pList2->ApplyUserColor(TRUE);
		m_pList3->ApplyUserColor(TRUE);

		INT nLOCNT = (INT)info.m_vLocals.size();
		for (INT j = 0; j<nLOCNT; ++j)
		{
			const LocalTerritory& local = info.m_vLocals[j];
			int nLine = m_pList->AddString("");
			m_pList->SetItemString(nLine, 1, local.m_strLocalName);
			m_pList->SetUserColor(nLine, 1, color);
			m_pList->SetColumnAlign(1, ALIGN_CENTER);


			m_pList->SetUserColor(nLine, 2, color);
			m_pList->SetImageIndex(nLine, 2, TRUE, local.m_bCountry);

			CString strStatusLoc;
			DWORD dwColorLoc = COLOR_CASTLE_NONE;
			switch (local.m_bStatus)
			{
			case BS_SKYGARDEN_START:
			case BS_NORMAL:
				strStatusLoc = CTChart::LoadString(TSTR_MISSION_NORMAL);
				break;
			case BS_BATTLE:
				strStatusLoc = CTChart::LoadString(TSTR_MISSION_WAR);
				dwColorLoc = COLOR_CASTLE_GEHBRA;
				break;
			case BS_PEACE:
				strStatusLoc = CTChart::LoadString(TSTR_MISSION_WAR_CLOSE);
				dwColorLoc = COLOR_CASTLE_TYCON;
				break;
			}
			m_pList->SetItemString(nLine, 3, strStatusLoc);
			m_pList->SetUserColor(nLine, 3, dwColorLoc);
			m_pList->SetColumnAlign(3, ALIGN_CENTER);

			m_pList->SetItemString(nLine, 4, local.m_strGuildName);
			m_pList->SetUserColor(nLine, 4, color);
			m_pList->SetColumnAlign(4, ALIGN_CENTER);

			DWORD dwInfoIndex = MAKELONG(j, i);
			m_pList->SetItemDataAllColumn(nLine, dwInfoIndex);
		}

		////CASTLE
		m_pList2->ApplyUserColor(TRUE);
		int nLine2 = m_pList2->AddString("");
		m_pList2->SetItemString(nLine2, 1, info.m_strCastleName);
		m_pList2->SetUserColor(nLine2, 1, color);
		m_pList2->SetColumnAlign(1, ALIGN_CENTER);

		m_pList2->SetItemString(nLine2, 2, info.m_strGuildName);
		m_pList2->SetUserColor(nLine2, 2, color);
		m_pList2->SetColumnAlign(2, ALIGN_CENTER);

		CString castletext;
		if (info.m_strAtkGuild != "" && info.m_strDefGuild == "")
			castletext.Format("%s", info.m_strAtkGuild);
		else if (info.m_strAtkGuild == "" && info.m_strDefGuild != "")
			castletext.Format("%s", info.m_strDefGuild);
		else if (info.m_strAtkGuild == "" && info.m_strDefGuild == "")
			castletext.Empty();
		else
			castletext.Format("%s  /  %s", info.m_strAtkGuild, info.m_strDefGuild);

		m_pList2->SetItemString(nLine2, 3, castletext);
		m_pList2->SetUserColor(nLine2, 3, color);
		m_pList2->SetColumnAlign(3, ALIGN_CENTER);

		CString strStatusCastle;
		DWORD dwColorCastle = COLOR_CASTLE_NONE;
		switch (info.m_bStatus)
		{
		case BS_SKYGARDEN_START:
		case BS_NORMAL:
			strStatusCastle = CTChart::LoadString(TSTR_MISSION_NORMAL);
			break;
		case BS_BATTLE:
			strStatusCastle = CTChart::LoadString(TSTR_MISSION_WAR);
			dwColorCastle = COLOR_CASTLE_GEHBRA;
			break;
		case BS_PEACE:
			strStatusCastle = CTChart::LoadString(TSTR_MISSION_WAR_CLOSE);
			dwColorCastle = COLOR_CASTLE_TYCON;
			break;
		}


		m_pList2->SetItemString(nLine2, 4, strStatusCastle);
		m_pList2->SetUserColor(nLine2, 4, dwColorCastle);
		m_pList2->SetColumnAlign(4, ALIGN_CENTER);

		DWORD dwInfoIndex2 = MAKELONG(0, i);
		m_pList2->SetItemDataAllColumn(nLine2, dwInfoIndex2);
		//	//CASTLE		
	}

	//MISSIONWAR
	m_pList3->ApplyUserColor(TRUE);
	CTGuildCommander::MissionVec::iterator itt, endd;
	itt = pCmd->m_MissionVec.begin();
	endd = pCmd->m_MissionVec.end();

	int nLine4 = m_pList3->AddString("");
	m_pList3->SetItemString(nLine4, 1, pCmd->m_SkyGarden.m_strMission);
	m_pList3->SetUserColor(nLine4, 1, COLOR_CASTLE_NONE);
	m_pList3->SetColumnAlign(1, ALIGN_CENTER);
	m_pList3->SetImageIndex(nLine4, 2, TRUE, pCmd->m_SkyGarden.m_bCountry);

	CString gardentext;
	DWORD roky = pCmd->m_SkyGarden.m_NextWar.GetYear();
	DWORD mesice = pCmd->m_SkyGarden.m_NextWar.GetMonth();
	DWORD dny = pCmd->m_SkyGarden.m_NextWar.GetDay();
	DWORD hodina = pCmd->m_SkyGarden.m_NextWar.GetHour();
	DWORD minuta = pCmd->m_SkyGarden.m_NextWar.GetMinute();
	if (hodina < 10)
		gardentext.Format("%d/%d/%d 0%d:%d", roky, mesice, dny, hodina, minuta);
	else if (minuta < 10)
		gardentext.Format("%d/%d/%d %d:0%d", roky, mesice, dny, hodina, minuta);
	else
		gardentext.Format("%d/%d/%d %d:%d", roky, mesice, dny, hodina, minuta);

	if (hodina < 10 && minuta < 10)
		gardentext.Format("%d/%d/%d 0%d:0%d", roky, mesice, dny, hodina, minuta);

	m_pList3->SetItemString(nLine4, 3, gardentext);
	m_pList3->SetUserColor(nLine4, 3, COLOR_CASTLE_NONE);
	m_pList3->SetColumnAlign(3, ALIGN_CENTER);

	CString strStatusSky;
	DWORD dwColorSky = COLOR_CASTLE_NONE;
	switch (pCmd->m_SkyGarden.m_bStatus)
	{
	case BS_SKYGARDEN_START:
	case BS_NORMAL:
		strStatusSky = CTChart::LoadString(TSTR_MISSION_NORMAL);
		break;
	case BS_BATTLE:
		strStatusSky = CTChart::LoadString(TSTR_MISSION_WAR);
		dwColorSky = COLOR_CASTLE_GEHBRA;
		break;
	case BS_PEACE:
		strStatusSky = CTChart::LoadString(TSTR_MISSION_WAR_CLOSE);
		dwColorSky = COLOR_CASTLE_TYCON;
		break;
	}

	m_pList3->SetItemString(nLine4, 4, strStatusSky);
	m_pList3->SetUserColor(nLine4, 4, dwColorSky);
	m_pList3->SetColumnAlign(4, ALIGN_CENTER);

	int ii = 0;
	for (; itt != endd; ++itt)
	{
		int nLine3 = m_pList3->AddString("");
		m_pList3->SetItemString(nLine3, 1, itt->m_strMission);
		m_pList3->SetUserColor(nLine3, 1, COLOR_CASTLE_NONE);
		m_pList3->SetColumnAlign(1, ALIGN_CENTER);

		m_pList3->SetImageIndex(nLine3, 2, TRUE, itt->m_bCountry);
		m_pList3->SetUserColor(nLine3, 2, COLOR_CASTLE_NONE);
		m_pList3->SetColumnAlign(2, ALIGN_CENTER);

		CString guildtext;
		DWORD rok = itt->m_NextWar.GetYear();
		DWORD mesic = itt->m_NextWar.GetMonth();
		DWORD den = itt->m_NextWar.GetDay();
		DWORD hodiny = itt->m_NextWar.GetHour();
		DWORD minuty = itt->m_NextWar.GetMinute();
		if (hodiny < 10)
			guildtext.Format("%d/%d/%d 0%d:%d", rok, mesic, den, hodiny, minuty);
		else if (minuty < 10)
			guildtext.Format("%d/%d/%d %d:0%d", rok, mesic, den, hodiny, minuty);
		else
			guildtext.Format("%d/%d/%d %d:%d", rok, mesic, den, hodiny, minuty);

		if (hodiny < 10 && minuty < 10)
			guildtext.Format("%d/%d/%d 0%d:0%d", rok, mesic, den, hodiny, minuty);

		m_pList3->SetItemString(nLine3, 3, guildtext);
		m_pList3->SetUserColor(nLine3, 3, COLOR_CASTLE_NONE);
		m_pList3->SetColumnAlign(3, ALIGN_CENTER);

		CString strStatus;
		DWORD dwColor = COLOR_CASTLE_NONE;
		switch (itt->m_bStatus)
		{
		case BS_SKYGARDEN_START:
		case BS_NORMAL:
			strStatus = CTChart::LoadString(TSTR_MISSION_NORMAL);
			break;
		case BS_BATTLE:
			strStatus = CTChart::LoadString(TSTR_MISSION_WAR);
			dwColor = COLOR_CASTLE_GEHBRA;
			break;
		case BS_PEACE:
			strStatus = CTChart::LoadString(TSTR_MISSION_WAR_CLOSE);
			dwColor = COLOR_CASTLE_TYCON;
			break;
		}

		m_pList3->SetItemString(nLine3, 4, strStatus);
		m_pList3->SetUserColor(nLine3, 4, dwColor);
		m_pList3->SetColumnAlign(4, ALIGN_CENTER);

		m_pList3->SetItemDataAllColumn(nLine3, ii++);
	}
}
// ====================================================================

// ====================================================================
void CTGuildWarInfoNewDlg::RequestInfo()
{
	CTGuildCommander::GetInstance()
		->RequestTerritoryList();
}
// --------------------------------------------------------------------
void CTGuildWarInfoNewDlg::ResetInfo()
{
	SetCurMode();
}
// ====================================================================

// ====================================================================
void CTGuildWarInfoNewDlg::OnLButtonDown(UINT nFlags, CPoint pt)
{
	SetCurMode();
	return;

	ITInnerFrame::OnLButtonDown(nFlags, pt);
}
// ====================================================================
void CTGuildWarInfoNewDlg::SwitchFocus(TComponent *pCandidate)
{
	if (m_pFocus == pCandidate)
		return;

	if (m_pFocus)
		m_pFocus->SetFocus(FALSE);

	if (pCandidate)
		pCandidate->SetFocus(TRUE);

	m_pFocus = pCandidate;
}
// ====================================================================
ITDetailInfoPtr CTGuildWarInfoNewDlg::GetTInfoKey(const CPoint& point)
{
	ITDetailInfoPtr pInfo;
	TListItem* pHITITEM = m_pList->GetHitItem(point);
	TListItem* pHITITEM2 = m_pList2->GetHitItem(point);
	TListItem* pHITITEM3 = m_pList3->GetHitItem(point);
	INT nIndex = m_pList->GetSel();
	INT nIndex2 = m_pList2->GetSel();
	INT nIndex3 = m_pList3->GetSel();

	CRect rc;
	rc.left = point.x;
	rc.top = point.y;
	rc.right = point.x + 32;
	rc.bottom = point.y + 32;

	int nIndexNew = m_pList->GetTop();
	int nIndexNew2 = m_pList2->GetTop();
	int nIndexNew3 = m_pList3->GetTop();


	if (pHITITEM || nIndex != -1)
	{
		if (m_pList && (DWORD)(m_pList) != (DWORD)(-1))
		{
			DWORD dwParam = 0;

			if (pHITITEM != NULL)
				dwParam = pHITITEM->m_param;
			else
				dwParam = m_pList->GetItemData(nIndexNew, 0);

			WORD dwCastleIndex = HIWORD(dwParam);
			WORD dwLocalIndex = LOWORD(dwParam);


			CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
			if (dwCastleIndex >= pCmd->GetCountTerritory())
				return pInfo;


			const CTGuildCommander::Territory& terr = pCmd->GetTerritory(dwCastleIndex);
			if (dwLocalIndex >= terr.m_vLocals.size())
				return pInfo;

			return  CTDetailInfoManager::NewTerritoryInst(
				terr.m_vLocals[dwLocalIndex].m_strLocalName,
				terr.m_vLocals[dwLocalIndex].m_dNextWar,
				terr.m_vLocals[dwLocalIndex].m_strHeroName,
				rc);
		}
	}

	if (pHITITEM2 || nIndex2 != -1)
	{
		if (m_pList2 && (DWORD)(m_pList2) != (DWORD)(-1))
		{
			DWORD dwParam = 0;

			if (pHITITEM2 != NULL)
				dwParam = pHITITEM2->m_param;
			else
				dwParam = m_pList2->GetItemData(nIndexNew2, 0);

			WORD dwCastleIndex2 = HIWORD(dwParam);
			WORD dwLocalIndex2 = LOWORD(dwParam);

			CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
			if (dwCastleIndex2 >= pCmd->GetCountTerritory())
				return pInfo;

			CTGuildCommander::Territory& terr = pCmd->GetTerritory(dwCastleIndex2);

			CString strMyGuild;
			CTClientChar* pMainChar = CTClientGame::GetInstance()->GetMainChar();
			if (pMainChar->m_dwGuildID)
				strMyGuild = pMainChar->m_strGUILD;
			else if (pMainChar->m_dwTacticsID)
				strMyGuild = pMainChar->m_strTACTICS;

			return CTDetailInfoManager::NewTerritoryInst(
				terr.m_strCastleName,
				terr.m_dlNextWar,
				terr.m_strAtkGuild,
#ifdef MODIFY_GUILD
				terr.m_strDefGuild,
#else
				terr.m_strGuildName,
#endif
				terr.m_wAtkGuildPoint,
				terr.m_wAtkCountryPoint,
				terr.m_bAtkCount,
				terr.m_wDefGuildPoint,
				terr.m_wDefCountryPoint,
				terr.m_bDefCount,
				strMyGuild,
				terr.m_wMyGuildPoint,
#ifdef MODIFY_GUILD
				&terr.m_vDTop3s,
				&terr.m_vCTop3s,
#endif
				rc);
		}
	}

	if (pHITITEM3 || nIndex3 != -1)
	{
		if (m_pList3 && (DWORD)(m_pList3) != (DWORD)(-1))
		{
			DWORD dwParam = 0;
			if (pHITITEM3 != NULL)
				dwParam = pHITITEM3->m_param;
			else
				dwParam = m_pList3->GetItemData(nIndexNew3, 0);

			CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
			if (dwParam >= pCmd->m_MissionVec.size())
			{
				return CTDetailInfoManager::NewMissionInst(
					pCmd->m_SkyGarden.m_strMission,
					pCmd->m_SkyGarden.m_NextWar,
					pCmd->m_SkyGarden.m_bCountry,
					pCmd->m_SkyGarden.m_bStatus,
					rc);
			}

			WORD dwCastleIndex = HIWORD(dwParam);
			WORD dwLocalIndex = LOWORD(dwParam);

			return CTDetailInfoManager::NewMissionInst(
				pCmd->m_MissionVec[dwParam].m_strMission,
				pCmd->m_MissionVec[dwParam].m_NextWar,
				pCmd->m_MissionVec[dwParam].m_bCountry,
				pCmd->m_MissionVec[dwParam].m_bStatus,
				rc);

		}
	}
	////
	//		TComponent* pPARENT = m_pList3->FindKid(49753);
	//	if( pPARENT->HitTest(point))
	//	{
	//		CTGuildCommander* pCmd = CTGuildCommander::GetInstance();
	//
	//		CRect rc;
	//		pPARENT->GetComponentRectWithKid( &rc);
	//		pPARENT->ComponentToScreen( &rc);
	//
	//		return CTDetailInfoManager::NewMissionInst(
	//			pCmd->m_SkyGarden.m_strMission,
	//			pCmd->m_SkyGarden.m_NextWar,
	//			pCmd->m_SkyGarden.m_bCountry,
	//			pCmd->m_SkyGarden.m_bStatus,
	//			rc);
	//	}

	return pInfo;
}
void CTGuildWarInfoNewDlg::ShowComponent(BOOL bVisible)
{
	ITInnerFrame::ShowComponent(bVisible);

	if (bVisible)
		SetCurMode();
}
