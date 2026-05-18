#include "stdafx.h"
#include "TClientGame.h"
#include "TCustomStrings.h"
#include "BattleRankProgressDlg.h"
#include "TCompanionInner.h"

#define MAX_RANK             (80)

CBattleRankProgressDlg::CBattleRankProgressDlg(TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc, CTMainUI* pMainUI)
: CTClientUIBase(pParent, pDesc)
{
	DWORD dwUseless[] = 
	{
		26964,
		26926,
		26963,
		26952,
		26925,
		21,
		875,
		11106,
		26965
	};

	for (BYTE i = 0; i < sizeof(dwUseless) / sizeof(DWORD); ++i)
	{
		TComponent* pBox = FindKid(dwUseless[i]);
		if (pBox)
		{
			RemoveKid(pBox);
			delete pBox;
		}
	}

	m_pRankPoint = FindKid(26953);

	CTCompanionDlg* pCompanion = (CTCompanionDlg*) CTClientGame::GetInstance()->GetFrame(TFRAME_COMPANION);
	m_pProgress = new TGauge(this, pCompanion->m_pHPBar->m_pDESC);
	m_pProgress->m_id = GetUniqueID();
	m_pProgress->MoveComponent(CPoint(153, 80));
	m_pProgress->SetStyle(TGS_GROW_UP | TS_CUSTOM_COLOR);
	m_pProgress->m_dwColor = 0xD0FF0A0A;
	AddKid(m_pProgress);

	m_pNextRank = new TImageList(this, pMainUI->m_pRankImage->m_pDESC);
	m_pNextRank->MoveComponent(CPoint(186, 110));
	m_pNextRank->m_id = GetUniqueID();
	AddKid(m_pNextRank);

	RemoveKid(m_pRankPoint);
	AddKid(m_pRankPoint);

	m_pRankPoint->MoveComponent(CPoint(12, 192));
	m_pRankPoint->SetTextAlign(ALIGN_CENTER);
	m_pRankPoint->SetFont(FindFont(22), NULL);

	for (TCOMP_LIST::iterator it = m_kids.begin(); it != m_kids.end(); ++it)
		(*it)->MoveComponentBy(-4, -3);

	m_pRankPoint->MoveComponentBy(91, -47);
	FindKid(19)->m_strText = "Next Rank";

	SetStyle(TS_CUSTOM_COLOR);
	m_dwColor = 0xE0FFFFFF;

	m_pMaxRankPoint = new TComponent(this, m_pRankPoint->m_pDESC);
	m_pMaxRankPoint->m_id = GetUniqueID();
	m_pMaxRankPoint->MoveComponentBy(-30, -35);
	m_pMaxRankPoint->SetTextAlign(ALIGN_CENTER);
	m_pMaxRankPoint->SetFont(FindFont(22), NULL);
	AddKid(m_pMaxRankPoint);

	m_pPlaceHolder[0] = new TComponent(this, m_pRankPoint->m_pDESC);
	m_pPlaceHolder[0]->m_id = GetUniqueID();
	m_pPlaceHolder[0]->MoveComponent(CPoint(40, 149));
	m_pPlaceHolder[0]->m_strText.Empty();
	AddKid(m_pPlaceHolder[0]);

	m_pPlaceHolder[1] = new TComponent(this, m_pRankPoint->m_pDESC);
	m_pPlaceHolder[1]->m_id = GetUniqueID();
	m_pPlaceHolder[1]->MoveComponent(CPoint(258, 76));
	m_pPlaceHolder[1]->m_strText.Empty();
	AddKid(m_pPlaceHolder[1]);
}

BOOL CBattleRankProgressDlg::CanWithItemUI()
{
	return TRUE;
}

CBattleRankProgressDlg::~CBattleRankProgressDlg()
{
}

void CBattleRankProgressDlg::SetInfo(DWORD dwCurPoint)
{
	BYTE bRank = CTChart::FindRank(dwCurPoint); bRank --;
	if (bRank >= MAX_RANK - 1)
		bRank = MAX_RANK - 2;

	DWORD dwNextPoint = CTChart::m_vBATTLERANK[bRank + 1];

	if (bRank >= 34)
		m_pNextRank->MoveComponent(CPoint(174, 103));
	else
		m_pNextRank->MoveComponent(CPoint(182, 107));

	DWORD dwLastPoint = 0;
	dwLastPoint = CTChart::m_vBATTLERANK[bRank];

	m_pProgress->SetGauge(dwCurPoint - dwLastPoint, dwNextPoint - dwLastPoint);
	m_pNextRank->SetCurImage(bRank + 1); // + 1
	m_pRankPoint->m_strText.Format("%d", dwCurPoint);
	m_pMaxRankPoint->m_strText.Format("%d", dwNextPoint);

	m_pEnable = NULL;
	UseOwnImages(25006);

	m_pPlaceHolder[0]->m_pEnable = NULL;
	m_pPlaceHolder[0]->UseOwnImages(25007);

	m_pPlaceHolder[1]->m_pEnable = NULL;
	m_pPlaceHolder[1]->UseOwnImages(25008);
}

void CBattleRankProgressDlg::OnLButtonUp(UINT nFlags, CPoint pt)
{
	if(FindKid(ID_CTRLINST_CLOSE)->HitTest(pt))
		ShowComponent(FALSE);

	return CTClientUIBase::OnLButtonUp(nFlags, pt);
}