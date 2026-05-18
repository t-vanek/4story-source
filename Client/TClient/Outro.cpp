#include "StdAfx.h"
#include "Outro.h"
#include "TClientGame.h"
#include "Resource.h"
#include "TClientWnd.h"

#define OUTRO_MAX	(BYTE) 22
#define IMAGE_SIZE	(INT) 1137

COutro::COutro(TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc)
	: CTClientUIBase(pParent, pDesc)
{
	m_pImage = FindKid(26966);
	m_pSoulLotteryWarn = FindKid(26876);
	m_pWarn2 = FindKid(26927);
	m_pLogoutWait = FindKid(11460);;

	m_pQuit = (TButton*)FindKid(26878);
	m_pQuit->ShowComponent(FALSE);
	m_pBackToGame = (TButton*)FindKid(26879);
	m_pBackToGame->m_strText = "Back to Game";

	m_bType = OUTRO_COUNT;
	m_pDevice = CTClientGame::GetInstance()->GetDevice();
}

COutro::~COutro()
{
}

void COutro::ShowComponent(BOOL bVisible)
{
	BYTE bImg = (rand() % OUTRO_MAX) + 1;
	CString strPath;
	strPath.Format(".\\Outro\\%d.tga", bImg);

	CTClientChar* pMainChar = CTClientGame::GetInstance()->GetMainChar();

	FLOAT fSoulExp = (FLOAT)pMainChar->GetSoulEXP();
	FLOAT fMaxExp = (FLOAT)pMainChar->m_dwNextEXP - pMainChar->m_dwPrevEXP;
	FLOAT fSoulPercent = (fSoulExp / fMaxExp) * 100.0f;

	m_pImage->LoadOwnIMG(strPath);
	m_pSoulLotteryWarn->m_strText.Format("Collect more soul power (%d%%) to receive your reward", 100 - (BYTE) fSoulPercent);
	m_pSoulLotteryWarn->SetTextAlign(ALIGN_CENTER);
	m_pWarn2->m_strText = "Your soul power and all your event will be reset if you leave the game!";
	m_pLogoutWait->m_strText.Empty();
	m_pQuit->m_strText = m_bType == OUTRO_CHARSEL ? "Character Select" : "Exit Game";

	INT nScreenX = CTClientGame::GetInstance()->GetScreenX();
	INT nScreenY = CTClientGame::GetInstance()->GetScreenY();

	CPoint pt = CTClientUIBase::m_vBasis[TBASISPOINT_CENTER_TOP];
	pt.x -= IMAGE_SIZE / 2;
	pt.y += 100;

	m_pImage->MoveComponent(pt);

	static const BYTE ComponentCount = 5;
	static INT BasisPoint[ComponentCount][4] =
	{
		{ (INT) m_pSoulLotteryWarn->m_id, TBASISPOINT_CENTER_MIDDLE, -510, 180 },
		{ (INT) m_pWarn2->m_id, TBASISPOINT_CENTER_MIDDLE, -400, 206},
		{ (INT) m_pLogoutWait->m_id, TBASISPOINT_CENTER_MIDDLE, 65, 270 + 59 },
		{ (INT) m_pQuit->m_id, TBASISPOINT_CENTER_MIDDLE, -120 / 2, 270 + 52 },
		{ (INT) m_pBackToGame->m_id, TBASISPOINT_CENTER_MIDDLE, -342 / 2, 270},
	};

	for (auto Component : m_kids)
	{
		for (BYTE i = 0; i < ComponentCount; ++i)
		{
			if (Component->m_id == BasisPoint[i][0])
			{
				CPoint pt = CTClientUIBase::m_vBasis[BasisPoint[i][1]];
				pt.x += BasisPoint[i][2];
				pt.y += BasisPoint[i][3];

				Component->MoveComponent(pt);
			}
		}
	}

	CTClientUIBase::ShowComponent(bVisible);
}

HRESULT COutro::Render(DWORD dwTickCount)
{
	if (m_bVisible) {
		CTClientGame* pGame = CTClientGame::GetInstance();
		if (pGame->m_bDoSelectCHAR || pGame->m_bDoEXIT)
			m_pLogoutWait->ShowComponent(TRUE);
		else
			m_pLogoutWait->ShowComponent(FALSE);

		FadeBack();
	}

	return CTClientUIBase::Render(dwTickCount);
}

void COutro::OnKeyUp(UINT nChar, int nRepCnt, UINT nFlags)
{
	if (nChar == VK_RETURN)
		ConfirmExit();
	else if (nChar == VK_ESCAPE)
		GoBack();

	return CTClientUIBase::OnChar(nChar, nRepCnt, nFlags);
}

void COutro::OnLButtonUp(UINT nFlags, CPoint pt)
{
	CTClientGame* pTGAME = CTClientGame::GetInstance();
	if (m_pBackToGame->HitTest(pt))
		GoBack();
	else if (m_pQuit->HitTest(pt))
		ConfirmExit();

	return CTClientUIBase::OnLButtonUp(nFlags, pt);
}

void COutro::GoBack()
{
	CTClientGame* pTGAME = CTClientGame::GetInstance();
	pTGAME->GetMainWnd()->SetMainFrame(pTGAME);
	pTGAME->RemoveKid(this);

	pTGAME->m_bDoSelectCHAR = FALSE;
	pTGAME->m_bDoEXIT = FALSE;
}

void COutro::ConfirmExit()
{
	CTClientGame* pTGAME = CTClientGame::GetInstance();
	if (m_bType == OUTRO_EXIT && !pTGAME->m_bDoEXIT)
	{
		pTGAME->m_dwLeftTickEXIT = TSELECT_EXITGAME_DELAY;
		pTGAME->m_bDoEXIT = TRUE;
	}
	else if (m_bType == OUTRO_CHARSEL && !pTGAME->m_bDoSelectCHAR)
	{
		pTGAME->m_dwLeftTickCHAR = TSELECT_CHARACTER_DELAY;
		pTGAME->m_bDoSelectCHAR = TRUE;
	}
	pTGAME->StopMoveMainChar();
}

void COutro::UpdateTick(DWORD dwTick)
{
	m_pLogoutWait->m_strText.Format("Your adventure ends in %d seconds", dwTick / 1000);
}

void COutro::FadeBack()
{
	static D3DXVECTOR3 vDRAWBACK[] =
	{
		D3DXVECTOR3(-1.0f, 1.0f, 0.5f),
		D3DXVECTOR3(1.0f, 1.0f, 0.5f),
		D3DXVECTOR3(-1.0f, -1.0f, 0.5f),
		D3DXVECTOR3(1.0f, -1.0f, 0.5f)
	};

	m_pDevice->m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	m_pDevice->m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
	m_pDevice->m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	m_pDevice->m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
	m_pDevice->m_pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	m_pDevice->m_pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	m_pDevice->m_pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	m_pDevice->m_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	m_pDevice->m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	m_pDevice->m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	m_pDevice->m_pDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	m_pDevice->m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	m_pDevice->m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	m_pDevice->m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	m_pDevice->m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	m_pDevice->m_pDevice->SetRenderState(D3DRS_TEXTUREFACTOR, 0xAA000000);

	D3DXMATRIX m;
	D3DXMatrixIdentity(&m);
	m_pDevice->m_pDevice->SetTransform(D3DTS_WORLD, &m);
	m_pDevice->m_pDevice->SetTransform(D3DTS_VIEW, &m);
	m_pDevice->m_pDevice->SetTransform(D3DTS_PROJECTION, &m);

	m_pDevice->m_pDevice->SetFVF(D3DFVF_XYZ);

	m_pDevice->m_pDevice->SetTexture(0, NULL);
	m_pDevice->m_pDevice->SetTexture(1, NULL);

	HRESULT hr = m_pDevice->m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vDRAWBACK, sizeof(D3DXVECTOR3));
}