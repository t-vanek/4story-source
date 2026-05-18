#include "stdafx.h"
#include "TBFGStatus.h"
#include "TClientGame.h"
#include "BRRankingDlg.h"

#define ID_CTRLINST_VALOP     (0x0000694E)
#define ID_CTRLINST_DEROP     (0x0000694F)
#define ID_CTRLINST_MISSI     (0x00006A68)
#define ID_CTRLINST_BOW_YES     (0x00006A69)
#define ID_CTRLINST_BOW_NO     (0x00006A6A)
#define ID_CTRLINST_BOW_FINISH     (0x00006DD9)
#define ID_BUTTON_BOWRANK         (0x0000694A)
#define ID_BUTTON_BOWSURR         (0x0000694B)
#define ID_FRAME_FQUEST         (0x00000459)
#define ID_BOWTIME_MIN1     (0x0000695B)
#define ID_BOWTIME_MIN2     (0x0000695A)
#define ID_BOWTIME_SEC1         (0x00006959)
#define ID_BOWTIME_SEC2         (0x00006958)


CTBFGStatus::CTBFGStatus( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
: CTClientUIBase( pParent, pDesc )
{
	static DWORD dwToHide[] = {
		27238,
		27239,
		27237,
		ID_CTRLINST_BOW_YES,
		ID_CTRLINST_BOW_NO,
		ID_CTRLINST_BOW_FINISH,
		ID_CTRLINST_MISSI,
		ID_BUTTON_BOWSURR
	};

	for (BYTE i = 0; i < sizeof(dwToHide) / sizeof(DWORD); ++i)
	{
		TComponent* pComponent = FindKid(dwToHide[i]);
		if(pComponent)
		{
			RemoveKid(pComponent);
			delete pComponent;
		}
	}

	m_pDefugel = static_cast<TComponent*>(FindKid(ID_CTRLINST_VALOP));
	m_pCraxion = static_cast<TComponent*>(FindKid(ID_CTRLINST_DEROP));
	m_pRank = static_cast<TButton*>(FindKid(ID_BUTTON_BOWRANK));

	m_p1Min = static_cast<TImageList*>(FindKid(ID_BOWTIME_MIN1));
	m_p2Min = static_cast<TImageList*>(FindKid(ID_BOWTIME_MIN2));
	m_p1Sec = static_cast<TImageList*>(FindKid(ID_BOWTIME_SEC1));
	m_p2Sec = static_cast<TImageList*>(FindKid(ID_BOWTIME_SEC2));

	m_pNation1 = FindKid(28119);
	m_pNation2 = FindKid(28120);

	m_pHeart = new TComponent(this, m_pNation1->m_pDESC);
	m_pSkull = new TComponent(this, m_pNation2->m_pDESC);

	m_pHeart->m_id = GetUniqueID();
	m_pSkull->m_id = GetUniqueID();
	
	CPoint pLeft, pRight;
	m_pNation1->GetComponentPos(&pLeft);
	m_pNation2->GetComponentPos(&pRight);

	pLeft.x += 20;
	pLeft.y += 15;
	pRight.x += 8;
	pRight.y += 8;

	m_pHeart->MoveComponent(pLeft);
	m_pSkull->MoveComponent(pRight);

	AddKid(m_pHeart);
	AddKid(m_pSkull);

	SetTime(0, 0);
	m_pDefugel->m_strText = "?";
	m_pCraxion->m_strText = "?";
}

CTBFGStatus::~CTBFGStatus()
{

}

BOOL CTBFGStatus::CanWithItemUI()
{
	return TRUE;
}

HRESULT CTBFGStatus::Render(DWORD dwTickCount)
{
	if (!IsVisible())
		return CTClientUIBase::Render(dwTickCount);

	BYTE bShowBR = m_bMode == MODE_BR;

	m_pHeart->ShowComponent(bShowBR);
	m_pSkull->ShowComponent(bShowBR);

	m_pNation1->ShowComponent(!bShowBR);
	m_pNation2->ShowComponent(!bShowBR);

	return CTClientUIBase::Render(dwTickCount);
}

void CTBFGStatus::SetTime(BYTE bMin, BYTE bSec)
{
	BYTE bMin1 = bMin / 10;
	BYTE bMin2 = bMin % 10;

	BYTE bSec1 = bSec / 10;
	BYTE bSec2 = bSec % 10;

	if(bMin <= 10)
	{
		m_p1Min->SetCurImage(0);
		m_p2Min->SetCurImage(bMin);
	}
	if(bSec <= 10)
	{
		m_p1Sec->SetCurImage(0);
		m_p2Sec->SetCurImage(bSec);
	}
	if(bMin >= 10)
	{
		m_p1Min->SetCurImage(bMin1);
		m_p2Min->SetCurImage(bMin2);
	}
	if(bSec >= 10)
	{
		m_p1Sec->SetCurImage(bSec1);
		m_p2Sec->SetCurImage(bSec2);
	}
}

void CTBFGStatus::SetPoints(BYTE bDefugelPoints, BYTE bCraxionPoints)
{
	m_pDefugel->m_strText.Format("%d", bDefugelPoints);
	m_pCraxion->m_strText.Format("%d", bCraxionPoints);
}

void CTBFGStatus::SetMode( BYTE bMode )
{
	if( m_bMode == bMode )
		return;

	m_bMode = bMode;
}

void CTBFGStatus::ShowComponent(BOOL bVisible)
{
	if (bVisible)
	{
		m_pHeart->m_pEnable = m_pSkull->m_pEnable = NULL;
		m_pHeart->UseOwnImages(25001);
		m_pSkull->UseOwnImages(25002);
	}

	return CTClientUIBase::ShowComponent(bVisible);
}

void CTBFGStatus::UpdateTopUI( BYTE bLifes, WORD wKills, WORD wAssists )
{
	m_pDefugel->m_strText.Format("%d", bLifes);

	if(wAssists)
		m_pCraxion->m_strText.Format("K:%d A:%d", wKills, wAssists);
	else
		m_pCraxion->m_strText.Format("%d", wKills);
}

void CTBFGStatus::OnLButtonUp( UINT nFlags, CPoint pt)
{
	if (IsVisible() && CTClientGame::GetInstance()->GetSession())
	{
		if (m_pRank->HitTest(pt))
		{
			if (CTClientGame::IsInBOWMap())
				CTClientGame::GetInstance()->GetSession()->SendCS_BOWRANKING_REQ();
			else if (CTClientGame::IsInBRMap())
				static_cast<CBRRankingDlg*>(CTClientGame::GetInstance()->GetFrame(TFRAME_BRRANKING))->ResetRank();
		}
	}

	CTClientUIBase::OnLButtonUp( nFlags, pt);
}

