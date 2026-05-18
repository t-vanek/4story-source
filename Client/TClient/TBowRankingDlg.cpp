#include "StdAfx.h"
#include "TBowRankingDlg.h"
#include "TClientGame.h"
#include "TBowRankClass.h"
#include "TBowGuildRankClass.h"

CTBowRankingDlg::CTBowRankingDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
: CTClientUIBase(pParent, pDesc)
{
	m_pNRanking[0] = FindKid(28134);
	m_pName[0] = FindKid(28138);
	m_pServer[0] = FindKid(28136);
	m_pBP[0] = FindKid(28135);
	m_pBB[0] = FindKid(28149);
	m_pCountry[0] = (TImageList*)FindKid(28152);

	m_pGCountry[0] = (TImageList*) FindKid(28153);
	m_pGNRanking[0] = FindKid(28140);
	m_pGBB[0] = FindKid(28151);
	m_pGTP[0] = FindKid(28146);
	m_pGServer[0] = FindKid(28144);
	m_pGName[0] = FindKid(28141);

	for (BYTE i = 1; i < BOW_RANK_COUNT; ++i)
	{
		CPoint pRanking, pName, pServer, pBP, pBB, pCountry;

		m_pNRanking[0]->GetComponentPos(&pRanking);
		m_pName[0]->GetComponentPos(&pName);
		m_pServer[0]->GetComponentPos(&pServer);
		m_pBP[0]->GetComponentPos(&pBP);
		m_pBB[0]->GetComponentPos(&pBB);
		m_pCountry[0]->GetComponentPos(&pCountry);

		pRanking.y += i * 27;
		pName.y += i * 27;
		pServer.y += i * 27;
		pBP.y += i * 27;
		pBB.y += i * 27;
		pCountry.y += i * 27;

		m_pNRanking[i] = new TComponent(this, m_pNRanking[0]->m_pDESC);
		m_pName[i] = new TComponent(this, m_pName[0]->m_pDESC);
		m_pServer[i] = new TComponent(this, m_pServer[0]->m_pDESC);
		m_pBP[i] = new TComponent(this, m_pBP[0]->m_pDESC);
		m_pBB[i] = new TComponent(this, m_pBB[0]->m_pDESC);
		m_pCountry[i] = new TImageList(this, m_pCountry[0]->m_pDESC);

		m_pNRanking[i]->m_id = GetUniqueID();
		m_pName[i]->m_id = GetUniqueID();
		m_pServer[i]->m_id = GetUniqueID();
		m_pBP[i]->m_id = GetUniqueID();
		m_pBB[i]->m_id = GetUniqueID();
		m_pCountry[i]->m_id = GetUniqueID();

		m_pNRanking[i]->MoveComponent(pRanking);
		m_pName[i]->MoveComponent(pName);
		m_pServer[i]->MoveComponent(pServer);
		m_pBP[i]->MoveComponent(pBP);
		m_pBB[i]->MoveComponent(pBB);
		m_pCountry[i]->MoveComponent(pCountry);

		AddKid(m_pNRanking[i]);
		AddKid(m_pName[i]);
		AddKid(m_pServer[i]);
		AddKid(m_pBP[i]);
		AddKid(m_pBB[i]);
		AddKid(m_pCountry[i]);

		m_pGNRanking[0]->GetComponentPos(&pRanking);
		m_pGName[0]->GetComponentPos(&pName);
		m_pGServer[0]->GetComponentPos(&pServer);
		m_pGTP[0]->GetComponentPos(&pBP);
		m_pGBB[0]->GetComponentPos(&pBB);
		m_pGCountry[0]->GetComponentPos(&pCountry);

		pRanking.y += i * 27;
		pName.y += i * 27;
		pServer.y += i * 27;
		pBP.y += i * 27;
		pBB.y += i * 27;
		pCountry.y += i * 27;

		m_pGNRanking[i] = new TComponent(this, m_pGNRanking[0]->m_pDESC);
		m_pGName[i] = new TComponent(this, m_pGName[0]->m_pDESC);
		m_pGServer[i] = new TComponent(this, m_pGServer[0]->m_pDESC);
		m_pGTP[i] = new TComponent(this, m_pGTP[0]->m_pDESC);
		m_pGBB[i] = new TComponent(this, m_pGBB[0]->m_pDESC);
		m_pGCountry[i] = new TImageList(this, m_pGCountry[0]->m_pDESC);

		m_pGNRanking[i]->m_id = GetUniqueID();
		m_pGName[i]->m_id = GetUniqueID();
		m_pGServer[i]->m_id = GetUniqueID();
		m_pGTP[i]->m_id = GetUniqueID();
		m_pGBB[i]->m_id = GetUniqueID();
		m_pGCountry[i]->m_id = GetUniqueID();

		m_pGNRanking[i]->MoveComponent(pRanking);
		m_pGName[i]->MoveComponent(pName);
		m_pGServer[i]->MoveComponent(pServer);
		m_pGTP[i]->MoveComponent(pBP);
		m_pGBB[i]->MoveComponent(pBB);
		m_pGCountry[i]->MoveComponent(pCountry);

		AddKid(m_pGNRanking[i]);
		AddKid(m_pGName[i]);
		AddKid(m_pGServer[i]);
		AddKid(m_pGTP[i]);
		AddKid(m_pGBB[i]);
		AddKid(m_pGCountry[i]);
	}

	m_pBB[0]->m_strText = "-";
	m_pGNRanking[0]->m_strText = "-";

	DWORD bStyle = GetStyle();
	SetStyle(bStyle |+ TS_CUSTOM_COLOR);

	m_dwColor = 0xCBFFFFFF;
}

CTBowRankingDlg::~CTBowRankingDlg()
{
	ReleaseData();
}

BOOL CTBowRankingDlg::Sorter(CTBowRank* pLevy, CTBowRank* pPravy)
{
	if(pLevy->m_dwSP < pPravy->m_dwSP)
		return FALSE;
	else if(pLevy->m_dwBB == pPravy->m_dwBB)
		if(pLevy->m_dwBB < pPravy->m_dwBB)
			return FALSE;
		else
			return TRUE;
	else
		return TRUE;

	return TRUE;
}

BOOL CTBowRankingDlg::SortGuilds(CTBowGRank* pLevy, CTBowGRank* pPravy)
{
	if(pLevy->m_dwSP < pPravy->m_dwSP)
		return FALSE;
	else if(pLevy->m_dwSP == pPravy->m_dwSP)
		if(pLevy->m_dwBB < pPravy->m_dwBB)
			return FALSE;
		else
			return TRUE;
	else
		return TRUE;

	return TRUE;
}

void CTBowRankingDlg::ReleaseData()
{
	for (BYTE i = 0; i < m_vTBODRANK.size(); ++i)
		delete m_vTBODRANK[i];

	for (BYTE i = 0; i < m_vTGUILDRANK.size(); ++i)
		delete m_vTGUILDRANK[i];

	m_vTBODRANK.clear();
	m_vTGUILDRANK.clear();
}

void CTBowRankingDlg::ResetRank()
{
	sort(m_vTGUILDRANK.begin(), m_vTGUILDRANK.end(), SortGuilds);
	sort(m_vTBODRANK.begin(), m_vTBODRANK.end(), Sorter);

	for (BYTE i = 0; i < BOW_RANK_COUNT; ++i)
	{
		m_pNRanking[i]->m_strText.Empty();
		m_pName[i]->m_strText.Empty();
		m_pServer[i]->m_strText.Empty();
		m_pBP[i]->m_strText.Empty();
		m_pBB[i]->m_strText.Empty();
		m_pCountry[i]->SetCurImage(-1);
	}

	for (size_t i = 0; i < m_vTBODRANK.size(); ++i)
	{
		if (m_vTBODRANK[i]->m_dwCharID == CTClientGame::GetInstance()->GetMainChar()->m_dwID)
		{
			m_pNRanking[0]->m_strText.Format("%d", i + 1);
			m_pBB[0]->m_strText.Format("%d", m_vTBODRANK[i]->m_dwBB);
			m_pName[0]->m_strText = m_vTBODRANK[i]->m_strName;
			m_pServer[0]->m_strText = "4Story";
			m_pBP[0]->m_strText.Format("%d", m_vTBODRANK[i]->m_dwSP);
			m_pCountry[0]->SetCurImage(m_vTBODRANK[i]->m_bCountry);
			break;
		}
	}

	if (m_vTBODRANK.size() > 3)
		m_vTBODRANK.resize(3);

	for (size_t i = 0; i < m_vTBODRANK.size(); ++i)
	{
		m_pNRanking[1 + i]->m_strText.Format("%d", 1 + i);
		m_pBB[1 + i]->m_strText.Format("%d", m_vTBODRANK[i]->m_dwBB);
		m_pName[1 + i]->m_strText = m_vTBODRANK[i]->m_strName;
		m_pServer[1 + i]->m_strText = "4Story";
		m_pBP[1 + i]->m_strText.Format("%d", m_vTBODRANK[i]->m_dwSP);
		m_pCountry[1 + i]->SetCurImage(m_vTBODRANK[i]->m_bCountry);
	}

	for (BYTE i = 0; i < BOW_RANK_COUNT; ++i)
	{
		m_pGNRanking[i]->m_strText.Empty();
		m_pGName[i]->m_strText.Empty();
		m_pGServer[i]->m_strText.Empty();
		m_pGTP[i]->m_strText.Empty();
		m_pGBB[i]->m_strText.Empty();
		m_pGCountry[i]->SetCurImage(-1);
	}

	for (size_t i = 0; i < m_vTGUILDRANK.size(); ++i)
	{
		if (m_vTGUILDRANK[i]->m_dwID == CTClientGame::GetInstance()->GetMainChar()->m_dwGuildID)
		{
			m_pGNRanking[0]->m_strText.Format("%d", i + 1);
			m_pGBB[0]->m_strText.Format("%d", m_vTGUILDRANK[i]->m_dwBB);
			m_pGName[0]->m_strText = m_vTGUILDRANK[i]->m_strName;
			m_pGServer[0]->m_strText = "4Story";
			m_pGTP[0]->m_strText.Format("%d", m_vTGUILDRANK[i]->m_dwSP);
			m_pGCountry[0]->SetCurImage(m_vTGUILDRANK[i]->m_bCountry);
			break;
		}
	}
	
	if (m_vTGUILDRANK.size() > 3)
		m_vTGUILDRANK.resize(3);

	for (size_t i = 0; i < m_vTGUILDRANK.size(); ++i)
	{
		m_pGBB[1 + i]->m_strText.Format("%d", m_vTGUILDRANK[i]->m_dwBB);
		m_pGName[1 + i]->m_strText = m_vTGUILDRANK[i]->m_strName;
		m_pGServer[1 + i]->m_strText = "4Story";
		m_pGTP[1 + i]->m_strText.Format("%d", m_vTGUILDRANK[i]->m_dwSP);
		m_pGNRanking[1 + i]->m_strText.Format("%d", 1 + i);
		m_pGCountry[1 + i]->SetCurImage(m_vTGUILDRANK[i]->m_bCountry);
	}
}