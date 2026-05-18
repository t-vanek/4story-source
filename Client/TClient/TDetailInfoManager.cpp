#include "stdafx.h"
#include "TDetailInfoManager.h"
#include "TClientGame.h"
#include "TClientWnd.h"

// ====================================================================================================
ITDetailInfoPtr		CTDetailInfoManager::m_pLastInfo(NULL);
DWORD				CTDetailInfoManager::m_dwInfoTick = 0;
DWORD				CTDetailInfoManager::m_dwInfoStaticTick = 0;
// ====================================================================================================
ITDetailInfoPtr CTDetailInfoManager::NewStatInst( DWORD dwInfoID, LPTSTATINFO pStatInfo, DWORD dwTitleColor, const CRect& rc)
{
	return ITDetailInfoPtr( new CTStatDetInfo(	 dwInfoID, pStatInfo, dwTitleColor, rc) );
}
// ====================================================================================================
ITDetailInfoPtr CTDetailInfoManager::NewNorInst(const CString& strTitle,
												DWORD dwInfoID, 
												WORD wImgID, 
												const CRect& rc)
{
	return ITDetailInfoPtr( new CTNorDetInfo(strTitle,dwInfoID,wImgID,rc) );
}
// ----------------------------------------------------------------------------------------------------
ITDetailInfoPtr CTDetailInfoManager::NewSkillInst(LPTSKILL pTSkill,
												  BYTE bLevel,
												  BOOL bSkillReq,
												  const CRect& rc)
{
	return ITDetailInfoPtr( new CTSkillDetInfo(pTSkill,bLevel,bSkillReq,rc) );
}
// ----------------------------------------------------------------------------------------------------
ITDetailInfoPtr CTDetailInfoManager::NewItemInst(LPTITEM pItem, const CRect& rc)
{
	if(pItem->m_bCanGrade == 1)
		return ITDetailInfoPtr( new CTItemDetInfo(TDEFINFO_TYPE_GEM_0,pItem, rc) );
	else
		return ITDetailInfoPtr( new CTItemDetInfo(TDETINFO_TYPE_ITEM,pItem, rc) );
}
// ----------------------------------------------------------------------------------------------------
ITDetailInfoPtr CTDetailInfoManager::NewItemInst(CTClientItem* pItem, const CRect& rc, BYTE bSecondInfo)
{
	ITDetailInfoPtr pInfo;

	if (!pItem)
		return pInfo;

	LPTITEM pTITEM = pItem->GetTITEM();
	if( !pTITEM )
		return pInfo;

	if(pTITEM->m_bType == IT_COMPANION || pTITEM->m_bKind == IK_PETTRANSFORM)
		return ITDetailInfoPtr(new CTCompanionDetInfo(TDEFINFO_TYPE_COMPANION, pItem, rc));
	else if(pTITEM->m_bType == IT_COSTUME)
		return ITDetailInfoPtr(new CTOptionItemDetInfo(TDETINFO_TYPE_OPTIONITEM, pItem, rc));
	else if(pTITEM->m_bKind == IK_BACK)
	{
		if(pItem->GetGem() == 0)
			return pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDEFINFO_TYPE_GEM_0, pItem, rc));
		else if(pItem->GetGem() == 1)
			return pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDEFINFO_TYPE_GEM_1, pItem, rc));
		else if(pItem->GetGem() == 2)
			return pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDEFINFO_TYPE_GEM_2, pItem, rc));
		else if(pItem->GetGem() == 3)
			return pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDEFINFO_TYPE_GEM_3, pItem, rc));
		else if(pItem->GetGem() == 4)
			return pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDEFINFO_TYPE_GEM_4, pItem, rc));
		else if(pItem->GetGem() == 5)
			return pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDEFINFO_TYPE_GEM_5, pItem, rc));
		else
			return ITDetailInfoPtr(new CTOptionItemDetInfo(TDETINFO_TYPE_OPTIONITEM, pItem, rc));
	}

	if( pTITEM->m_bType != IT_GAMBLE &&
		pItem->CanGamble() )
	{
		pInfo = ITDetailInfoPtr(new CTSealedItemDetInfo(TDETINFO_TYPE_SEALEDITEM, pItem, rc));
	}
	else
	{
		if( pItem->GetQuality() & TITEM_QUALITY_NORMAL )
		{
			if(pItem->GetTITEM()->m_bCanGrade == 1)
			{
				if(pItem->GetWrap())
					pInfo = ITDetailInfoPtr(new CTItemInstDetInfo(TDEFINFO_TYPE_GEM_0_W, pItem, rc));
				else
					pInfo = ITDetailInfoPtr(new CTItemInstDetInfo(TDEFINFO_TYPE_GEM_0, pItem, rc));
			}
			else
				pInfo = ITDetailInfoPtr(new CTItemInstDetInfo(TDEFINFO_TYPE_INSTITEM, pItem, rc));

		}
		else
			if(pItem->GetTITEM()->m_bCanGrade == 1 || pItem->GetGem() > 0)
			{
				if(pItem->GetWrap())
				{
					if(pItem->GetGem() == 0)
						pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDEFINFO_TYPE_GEM_0_W, pItem, rc));
					else if(pItem->GetGem() == 1)
						pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDEFINFO_TYPE_GEM_1_W, pItem, rc));
					else if(pItem->GetGem() == 2)
						pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDEFINFO_TYPE_GEM_2_W, pItem, rc));
					else if(pItem->GetGem() == 3)
						pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDEFINFO_TYPE_GEM_3_W, pItem, rc));
					else if(pItem->GetGem() == 4)
						pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDEFINFO_TYPE_GEM_4_W, pItem, rc));
					else if(pItem->GetGem() == 5)
						pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDEFINFO_TYPE_GEM_5_W, pItem, rc));
				}
				else
				{
					if(pItem->GetGem() == 0)
						pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDEFINFO_TYPE_GEM_0, pItem, rc));
					else if(pItem->GetGem() == 1)
						pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDEFINFO_TYPE_GEM_1, pItem, rc));
					else if(pItem->GetGem() == 2)
						pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDEFINFO_TYPE_GEM_2, pItem, rc));
					else if(pItem->GetGem() == 3)
						pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDEFINFO_TYPE_GEM_3, pItem, rc));
					else if(pItem->GetGem() == 4)
						pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDEFINFO_TYPE_GEM_4, pItem, rc));
					else if(pItem->GetGem() == 5)
						pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDEFINFO_TYPE_GEM_5, pItem, rc));
				}
			}
			else
				pInfo = ITDetailInfoPtr(new CTOptionItemDetInfo(TDETINFO_TYPE_OPTIONITEM, pItem, rc));
	}

	return pInfo;
}
// ====================================================================================================
ITDetailInfoPtr CTDetailInfoManager::NewTerritoryInst(
		CString strCastle,
		CTime timeNext,
		CString strAtkGuild,
		CString strDefGuild,
		WORD wAtkGuildPoint,
		WORD wAtkCountryPoint,
		BYTE bAtkCount,
		WORD wDefGuildPoint,
		WORD wDefCountryPoint,
		BYTE bDefCount,
		CString strMyGuild,
		WORD wMyGuildPoint,
#ifdef MODIFY_GUILD
		VTOP3* vDTop3,
		VTOP3* vCTop3,
#endif
		const CRect& rc)
{
	return ITDetailInfoPtr( new CTCastleDefInfo(
		strCastle,
		timeNext,
		strAtkGuild,
		strDefGuild,
		wAtkGuildPoint,
		wAtkCountryPoint,
		bAtkCount,
		wDefGuildPoint,
		wDefCountryPoint,
		bDefCount,
		strMyGuild,
		wMyGuildPoint,
#ifdef MODIFY_GUILD
		vDTop3,
		vCTop3,
#endif
		rc) );
}

ITDetailInfoPtr CTDetailInfoManager::NewTerritoryInst(
								CString strTerritory,
								 CTime timeNext,
								 CString strHeroName,
								 const CRect& rc )
{
	return ITDetailInfoPtr( new CTTerritoryDetInfo(strTerritory, timeNext, strHeroName, rc) );
}

ITDetailInfoPtr CTDetailInfoManager::NewPvPInst(
	BYTE bTabIndex,
	CString strName,
	BYTE bWin,
	BYTE bClass,
	BYTE bLevel,
	DWORD dwPoint,
	CTime dlDate,
	DWORD dwTitleColor,
	const CRect& rc )
{
	return ITDetailInfoPtr( new CTPvPDetInfo(
		bTabIndex,
		strName,
		bWin,
		bClass,
		bLevel,
		dwPoint,
		dlDate,
		dwTitleColor,
		rc ) );
}
// ====================================================================================================
ITDetailInfoPtr CTDetailInfoManager::NewTextInst(
	CString& strText,
	CPoint pt )
{
	return ITDetailInfoPtr( new CTTextDetInfo( strText, pt ) );
}

// ----------------------------------------------------------------------------------------------------
ITDetailInfoPtr CTDetailInfoManager::NewDefTooltipInst( CString strTitle, CString strTooltip, const CRect& rc )
{
	return ITDetailInfoPtr( new CTDefToolTipInfo( strTitle, strTooltip, rc ) );
}
// ----------------------------------------------------------------------------------------------------
ITDetailInfoPtr CTDetailInfoManager::NewAfterMathToolTip( CString strTitle, CString strAfter, CString strAfter2, const CRect& rc )
{
	return ITDetailInfoPtr( new CTAfterMathInfo( strTitle, strAfter, strAfter2,  rc ) );
}

ITDetailInfoPtr CTDetailInfoManager::NewBRTeamsToolTip(const CString& strTitle, const CString* strPlayer, const CRect& rc)
{
	return ITDetailInfoPtr(new CTBRTeamInfo(strTitle, strPlayer, rc));
}
// ----------------------------------------------------------------------------------------------------

void CTDetailInfoManager::UpdateTick(DWORD dwTick)
{
	if( m_pLastInfo )
	{
		if( m_dwInfoTick < TDETAIL_INFO_TICK )
			m_dwInfoTick += dwTick;
	}
	else
	{
		if( m_dwInfoTick > (dwTick>>1) )
			m_dwInfoTick -= (dwTick>>1);
		else
			m_dwInfoTick = 0;
	}

	m_dwInfoStaticTick -= min(m_dwInfoStaticTick, dwTick);
}
// ----------------------------------------------------------------------------------------------------
void CTDetailInfoManager::UpdateInfo(const CPoint& ptMouse)
{
	if(ptMouse.x == 0 && ptMouse.y == 0)
		return;

	CTClientWnd* pMainWnd = CTClientWnd::GetInstance();
	CTClientGame* pGame = CTClientGame::GetInstance();
	if( !pGame || !pMainWnd )
		return;

	if( m_dwInfoStaticTick != 0 )
		return ;

	ITDetailInfoPtr pCurInfo;

	if( pMainWnd->GetTMainFrame() == pGame )
	{
		TComponent* pHIT = pGame->GetTopFrame(ptMouse,0);
		if( !pHIT )
			pHIT = pGame->GetHitPartyItemLottery(ptMouse);

		if( pHIT )
		{
			CTClientUIBase* pFrame = static_cast<CTClientUIBase*>(pHIT);
			pCurInfo = pFrame->GetTInfoKey(ptMouse);
			if( pCurInfo && (!m_pLastInfo || !m_pLastInfo->Compare(pCurInfo.get())) )
			{
				m_pLastInfo = pCurInfo;
				pGame->DisableUI(TFRAME_DETAIL_INFO);
				pGame->DisableUI(TFRAME_EQUIP_DETAIL_INFO);
				pGame->DisableUI(TFRAME_EQUIP_DETAIL_INFO_2);
			}
			
		}

	}

	if( !pCurInfo )
	{
		m_pLastInfo = ITDetailInfoPtr(NULL);
		pGame->DisableUI(TFRAME_DETAIL_INFO);
		pGame->DisableUI(TFRAME_EQUIP_DETAIL_INFO);
		pGame->DisableUI(TFRAME_EQUIP_DETAIL_INFO_2);

		return;
	}


	CTDetailInfoDlg* pDetInfoDlg 
		= static_cast<CTDetailInfoDlg*>( pGame->GetFrame(TFRAME_DETAIL_INFO) );


	if( m_pLastInfo &&
		m_dwInfoTick >= TDETAIL_INFO_TICK && 
		!pDetInfoDlg->IsVisible() )
	{
		pDetInfoDlg->ResetINFO(m_pLastInfo);

		pGame->EnableUI(TFRAME_DETAIL_INFO);

		CRect rc;
		pDetInfoDlg->GetComponentRect(&rc);

		CPoint pt = m_pLastInfo->GetUIPosition(rc, ptMouse);
		pDetInfoDlg->MoveComponent(pt);


		CTDetailInfoDlg* pDetailInfo = 
			static_cast<CTDetailInfoDlg*>( pGame->GetFrame(TFRAME_EQUIP_DETAIL_INFO) );

		CTDetailInfoDlg* pDetailInfo2 = 
			static_cast<CTDetailInfoDlg*>( pGame->GetFrame(TFRAME_EQUIP_DETAIL_INFO_2) );

		if( pDetInfoDlg->IsVisible() && pCurInfo && !pDetailInfo->IsVisible() && pCurInfo->GetCanHandleSecondInfo() &&
			(pCurInfo->GetType() == TDEFINFO_TYPE_GEM_0 || pCurInfo->GetType() == TDEFINFO_TYPE_GEM_1 ||
			pCurInfo->GetType() == TDEFINFO_TYPE_GEM_2 || pCurInfo->GetType() == TDEFINFO_TYPE_GEM_3 ||
			pCurInfo->GetType() == TDEFINFO_TYPE_GEM_4 || pCurInfo->GetType() == TDEFINFO_TYPE_GEM_5 ||
			pCurInfo->GetType() == TDEFINFO_TYPE_GEM_0_W || pCurInfo->GetType() == TDEFINFO_TYPE_GEM_1_W ||
			pCurInfo->GetType() == TDEFINFO_TYPE_GEM_2_W || pCurInfo->GetType() == TDEFINFO_TYPE_GEM_3_W ||
			pCurInfo->GetType() == TDEFINFO_TYPE_GEM_4_W || pCurInfo->GetType() == TDEFINFO_TYPE_GEM_5_W ||
			pCurInfo->GetType() == TDETINFO_TYPE_ITEM || pCurInfo->GetType() == TDETINFO_TYPE_SEALEDITEM ||
			pCurInfo->GetType() == TDETINFO_TYPE_OPTIONITEM) )
		{
			CTClientChar* pMainChar = pGame->GetMainChar();
			if( !pMainChar )
				return;

			MAPTINVEN::iterator jirk;
			MAPTITEM::iterator jirki;
			BYTE bCount = 0;
			CTClientItem* pItems[2];

			CTItemInstDetInfo* pItem = static_cast<CTItemInstDetInfo*>(pCurInfo.get());
			if( !pItem )
				return;

			MAPTINVEN::iterator itTINVEN = pMainChar->m_mapTINVEN.find( INVEN_EQUIP );
			if( itTINVEN != pMainChar->m_mapTINVEN.end() )
			{
				for( jirki = (*itTINVEN).second->m_mapTITEM.begin(); jirki != (*itTINVEN).second->m_mapTITEM.end(); ++jirki)
				{
					if( (*jirki).second && pItem->GetItemInst() )
						if( (*jirki).second->GetTITEM()->m_dwSlotID == pItem->GetItemInst()->GetTITEM()->m_dwSlotID )
						{
							pItems[bCount] = (*jirki).second;
							bCount++;

							if(bCount == 2)
								break;
						}
				}
			}

			CPoint pOldPoint, pNewPoint;
			BYTE bModulus = FALSE;
			CRect rcRef = pCurInfo->GetSize();

			if( pItems[0] && bCount )
			{
				ITDetailInfoPtr pInfo = CTDetailInfoManager::NewItemInst( pItems[ 0 ], CRect(0,0,0,0), TRUE );
				pDetInfoDlg->GetComponentPos( &pOldPoint );
				pNewPoint = pOldPoint;

				pGame->EnableUI(TFRAME_EQUIP_DETAIL_INFO);
				pDetailInfo->ResetINFO( pInfo );

				if( pOldPoint.x == rcRef.left - pDetInfoDlg->m_rc.Width() )
				{
					bModulus = 1;
					pNewPoint.x -= pDetInfoDlg->m_rc.Width() + 6;
				}
				else
				{
					bModulus = 2; 
					pNewPoint.x += pDetInfoDlg->m_rc.Width() + 6;
				}

				if( (INT) (pOldPoint.x - (pDetailInfo->m_rc.Width() * bCount)) < 0 && bModulus == 1 )
					pNewPoint.x = rcRef.right;
				else if ( (pOldPoint.x + pDetailInfo->m_rc.Width() + (pDetInfoDlg->m_rc.Width() * bCount)) > pGame->GetScreenX() && bModulus == 2 )
					pNewPoint.x = rcRef.left - pDetailInfo->m_rc.Width();

				pDetailInfo->MoveComponent(pNewPoint);
			}

			if( bCount == 2 && pItems[ 1 ] )
			{
				ITDetailInfoPtr pInfo = CTDetailInfoManager::NewItemInst( pItems[ 1 ], CRect(0,0,0,0), TRUE );
				CPoint pSecPoint, pSecNewPoint, pFirstPoint;

				pGame->EnableUI(TFRAME_EQUIP_DETAIL_INFO_2);
				pDetailInfo2->ResetINFO( pInfo );
				pDetailInfo->GetComponentPos( &pSecPoint );
				pSecNewPoint = pSecPoint;

				pSecNewPoint.x += pDetailInfo2->m_rc.Width() + 6;
				if( pSecNewPoint.x + pDetailInfo2->m_rc.Width() > pGame->GetScreenX() )
				{
					if( rcRef.left - pDetailInfo->m_rc.Width() == pOldPoint.x )
						pSecNewPoint.x = pOldPoint.x - pDetailInfo2->m_rc.Width() - 6;
				}

				if( pNewPoint.x < pOldPoint.x )
					pSecNewPoint.x = pNewPoint.x - pDetailInfo2->m_rc.Width() - 6;
				
				pDetailInfo2->MoveComponent( pSecNewPoint );
			}
		}
	}
}

// ====================================================================================================
ITDetailInfoPtr CTDetailInfoManager::NewFameInst(
	CString	strName,
	DWORD	dwTotalPoint,
	DWORD	dwMonthPoint,
	WORD	wWin,
	WORD	wLose,
	BYTE	bLevel,
	BYTE	bClass,
	const CRect& rc )
{
	return ITDetailInfoPtr( new CTFameRankDetInfo(
		strName,
		dwTotalPoint,
		dwMonthPoint,
		wWin,
		wLose,
		bLevel,
		bClass,
		rc ) );
}
// ====================================================================================================

ITDetailInfoPtr CTDetailInfoManager::NewTournamentPlayerInst(
	CString strName,
	BYTE bWin,
	BYTE bCountry,
	BYTE bClass,
	BYTE bLevel,
	const CRect& rc)
{
	return ITDetailInfoPtr( new CTTournamentPlayerInfo(
		strName,
		bWin,
		bCountry,
		bClass,
		bLevel,
		rc ) );
}

ITDetailInfoPtr CTDetailInfoManager::NewTournamentPlayerInst(
	CString strTitle,
	CString strText,
	int nLine,
	const CRect& rc)
{
	return ITDetailInfoPtr( new CTTournamentPlayerInfo(
		strTitle,
		strText,
		nLine,
		rc ) );
}

ITDetailInfoPtr CTDetailInfoManager::NewMissionInst(
	CString strTerritory,
	CTime timeNext,
	BYTE bCountry,
	BYTE bStatus,
	const CRect& rc )
{
	return ITDetailInfoPtr( new CTMissionDetInfo(
		strTerritory,
		timeNext,
		bCountry,
		bStatus,
		rc ) );
}

ITDetailInfoPtr CTDetailInfoManager::NewPlayerInst(
	CString strName,
	BYTE bLevel,
	BYTE bRace,
	BYTE bSex,
	BYTE bFace,
	BYTE bHair,
	BYTE bSenior,
	BYTE bClass,
	const CRect& rc )
{
	return ITDetailInfoPtr( new CTPlayerDetInfo(
		strName,
		bLevel,
		bRace,
		bSex,
		bFace,
		bHair,
		bSenior,
		bClass,
		rc ));
}