// QuestMission.cpp: implementation of the CQuestMission class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <SvrInc.h>
#include "TMapSvrModule.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CQuestMission::CQuestMission()
{
}

CQuestMission::~CQuestMission()
{
}

void CQuestMission::ExecQuest(CTPlayer *pPlayer,
	DWORD dwTick,
	LPMAPMAPVQUESTTEMP pTRIGGER,
	LPMAPQUESTTEMP pQUESTTEMP,
	LPMAPTITEMTEMP pITEMTEMP,
	LPMAPTSKILLTEMP pSKILLTEMP,
	LPMAPTMONSTERTEMP pMONTEMP,
	LPMAPCLASS pCLASS)
{
	BYTE bLevel;
	if (!pPlayer->CanRunQuest(m_pQUEST, dwTick, bLevel))
	{

		MAPQUEST::iterator finder = pPlayer->m_mapQUEST.find(m_pQUEST->m_dwQuestID);
		CQuest *pQuest = NULL;

		if (finder == pPlayer->m_mapQUEST.end())
		{
			pQuest = CreateQuest(m_pQUEST);
			pPlayer->m_mapQUEST.insert(MAPQUEST::value_type(m_pQUEST->m_dwQuestID, pQuest));
		}
		else
			pQuest = (*finder).second;

		DWORD dwLevel = pPlayer->GetQuestLevel(m_pQUEST);
		if (dwLevel)
			pPlayer->m_mapLevelQuest.insert(MAPDWORD::value_type(dwLevel, m_pQUEST->m_dwQuestID));

		pPlayer->SendCS_QUESTADD_ACK(
			m_pQUEST->m_dwQuestID,
			m_pQUEST->m_bType);


		if(pQuest->m_bTriggerCount == 0xFF)
		{
			pQuest->m_bTriggerCount = 1;
			pQuest->m_bCompleteCount = 1;
		}

		pQuest->m_bTriggerCount++;
		if (pQuest->m_pQUEST->m_bType != QT_GUILD)
			pQuest->m_bSave = TRUE; 

		for (DWORD tc = 0; tc < pQuest->m_pQUEST->m_vTerm.size(); tc++)
			pPlayer->CheckQuest(
				dwTick,
				pQuest->m_pQUEST->m_vTerm[tc]->m_dwTermID,
				pQuest->m_pQUEST->m_vTerm[tc]->m_bTermType,
				0,
				0);


		WORD wItemID = 0;
		BYTE bCount = 0;
		BOOL Give = TRUE;
		for (auto Term : pQuest->m_pQUEST->m_vTerm)
		{
			if (Term->m_bTermType == QTT_GETITEM)
			{
				for (auto Term2 : pQuest->m_pQUEST->m_vTerm)
				{
					if (Term2->m_bTermType == QTT_ITEMID && Term->m_dwTermID == Term2->m_dwTermID)
					{
						for (auto Inven : pPlayer->m_mapTINVEN)
						{
							if (!Inven.second)
								continue;

							for (auto Item : Inven.second->m_mapTITEM)
							{
								if (!Item.second)
									continue;

								if (Item.second->m_wItemID == Term->m_dwTermID)
									Give = FALSE;
							}
						}

						if (!Give)
							continue;

						VTITEM vTITEM;
						vTITEM.clear();

						wItemID = Term->m_dwTermID;
						bCount = Term->m_bCount;
						MAPTITEMTEMP::iterator finder = pITEMTEMP->find(wItemID);
						if (bCount && finder != pITEMTEMP->end())
						{
							CTItem *pITEM = new CTItem();
							pITEM->m_dlID = _AtlModule.GenItemID();
							pITEM->m_pTITEM = (*finder).second;
							_AtlModule.SetItemAttr(pITEM, 0);
							pITEM->m_wItemID = wItemID;
							pITEM->m_bCount = bCount;
							pITEM->SetDuration(FALSE);
							vTITEM.push_back(pITEM);
						}

						VWORD vItemID;
						VBYTE vCount;
						vItemID.clear();
						vCount.clear();

						if (!vTITEM.empty())
						{
							if (pPlayer->CanPush(&vTITEM, 0))
							{
								for (DWORD k = 0; k < vTITEM.size(); k++)
								{
									if (!vTITEM[k]->m_bCount)
										continue;

									vItemID.push_back(vTITEM[k]->m_pTITEM->m_wItemID);
									vCount.push_back(vTITEM[k]->m_bCount);
								}

								pPlayer->PushTItem(&vTITEM);

								for (size_t k = 0; k < vItemID.size(); k++)
								{
									_AtlModule.CheckQuest(
										pPlayer,
										0,
										pPlayer->m_fPosX,
										pPlayer->m_fPosY,
										pPlayer->m_fPosZ,
										vItemID[k],
										QTT_GETITEM,
										TT_GETITEM,
										vCount[k]);
								}

								pPlayer->SendCS_MONITEMTAKE_ACK(MIT_SUCCESS);
							}
							else
							{
								pPlayer->DropQuest(m_pQUEST->m_dwParentID);
								pPlayer->SendCS_QUESTCOMPLETE_ACK(
									QR_INVENTORYFULL,
									m_pQUEST->m_dwParentID,
									0, 0,
									m_pQUEST->m_dwParentID);

								while (!vTITEM.empty())
								{
									delete vTITEM.back();
									vTITEM.pop_back();
								}
							}
						}

						vTITEM.clear();
						vItemID.clear();
						vCount.clear();

						break;
					}
				}
			}
		}
	
	for (INT i = 0; i < INT(pQuest->m_pQUEST->m_vTerm.size()); i++)
		if (pQuest->m_pQUEST->m_vTerm[i]->m_bTermType == QTT_TIMER)
		{
			pQuest->m_dwTick = pQuest->m_pQUEST->m_vTerm[i]->m_dwTermID;
			pQuest->m_dwBeginTick = dwTick;

			pPlayer->SendCS_QUESTSTARTTIMER_ACK(
				pQuest->m_pQUEST->m_dwQuestID,
				pQuest->m_dwTick);
		}

	CQuest::ExecQuest(
		pPlayer,
			dwTick,
			pTRIGGER,
			pQUESTTEMP,
			pITEMTEMP,
			pSKILLTEMP,
			pMONTEMP,
			pCLASS);
	}
}
