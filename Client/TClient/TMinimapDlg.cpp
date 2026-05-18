#include "StdAfx.h"
#include "TClientGame.h"

#ifdef NEW_IF
#include "TPotionPannel.h"
#endif

LPDIRECT3DTEXTURE9 CTMinimapDlg::m_pTMINIMAP = NULL;
LPDIRECT3DTEXTURE9 CTMinimapDlg::m_pTMASK = NULL;


CTMinimapDlg::CTMinimapDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc)
:CTClientUIBase( pParent, pDesc)
{
#ifdef NEW_IF
	static DWORD dwCompID[] = {
		ID_CTRLINST_FRAME, 
		ID_CTRLINST_MINIMAP_NEW,

		ID_CTRLINST_ZOOM
	}; 

	for( BYTE i=0; i<TMINIMAPCOMP_COUNT; i++)
		m_vTCOMP[i] = FindKid(dwCompID[i]);

	m_bMinimize = FALSE;
	m_bSND = FALSE;

	m_pDevice = NULL;
	m_pTMON = NULL;
	m_pHost = NULL;
	m_pTMAP = NULL;
	m_pCAM = NULL;






	m_pArea = FindKid(ID_CTRLINST_AREA);
	m_pArea->GetComponentRect(&m_rcAREA);

	m_pTZOOMSCROLL = (TScroll*) m_vTCOMP[TMINIMAPCOMP_ZOOM];
	m_pTZOOMSCROLL->SetScrollPos((INT)TMINIMAP_SCALE_COUNT, (INT)TMINIMAP_SCALE_COUNT);
#else
	static DWORD dwCompID[] = {
		ID_CTRLINST_MAX, //ID_CTRLINST_FRAME
		ID_CTRLINST_WORLD, //
		ID_CTRLINST_MINIMAP, //26272
		ID_CTRLINST_ZOOM}; //"

	for( BYTE i=0; i<TMINIMAPCOMP_COUNT; i++)
		m_vTCOMP[i] = FindKid(dwCompID[i]);

	m_pTITLE = FindKid(ID_CTRLINST_MAX_TITLE); //TITLE
	m_pMINMAXBTN = FindKid(ID_CTRLINST_MAX_BTN); //MINIMIZE
	m_pTOPFRAME = FindKid(ID_CTRLINST_MIN); //Opframe

	m_bMinimize = FALSE;
	m_bSND = FALSE;

	m_pDevice = NULL;
	m_pTMON = NULL;
	m_pHost = NULL;
	m_pTMAP = NULL;
	m_pCAM = NULL;

	TComponent *pTAREA = FindKid(ID_CTRLINST_AREA);
	pTAREA->GetComponentRect(&m_rcAREA);

	RemoveKid(pTAREA);
	delete pTAREA;

	m_pTZOOMSCROLL = (TScroll*) m_vTCOMP[TMINIMAPCOMP_ZOOM];
	m_pTZOOMSCROLL->SetScrollPos((INT)TMINIMAP_SCALE_COUNT, (INT)TMINIMAP_SCALE_COUNT);
#endif
}

CTMinimapDlg::~CTMinimapDlg()
{
}

void CTMinimapDlg::ResetVisible()
{
	for( BYTE i=0; i<TMINIMAPCOMP_COUNT; i++)
	{
#ifndef NEW_IF
		m_vTCOMP[i]->EnableComponent(!m_bMinimize);
#endif
		m_vTCOMP[i]->ShowComponent(!m_bMinimize);
	}
}

BOOL CTMinimapDlg::CanWithItemUI()
{
	return TRUE;
}

BOOL CTMinimapDlg::DoMouseWheel( UINT nFlags, short zDelta, CPoint pt)
{
	if( m_bMinimize )
		return FALSE;

	CRect rc = m_rc;
	ComponentToScreen(&rc);
	if(!rc.PtInRect(pt))
		return FALSE;

	int nRange, nPos;
	m_pTZOOMSCROLL->GetScrollPos(nRange, nPos);

	nPos += zDelta > 0 ? -1 : 1;
	nPos = min( max( nPos, 0), nRange);
	m_pTZOOMSCROLL->SetScrollPos(nRange, nPos);

	return TRUE;
}

BOOL CTMinimapDlg::HitTest( CPoint pt)
{
#ifdef NEW_IF
	LPIMAGESET pImageset = m_pArea->m_pEnable;

	if(!CTClientGame::GetInstance()->GetMainChar()->CountMaintainFunc( SDT_AI, SDT_RUNAWAY, 1))
	{
		if( pImageset )
		{
			pImageset->m_dwCurTick = m_pArea->m_dwTotalTick % pImageset->m_dwTotalTick;
			CD3DImage *pImage = pImageset->GetImage();

			if( pImage )
			{
				CPoint point = pt;

				m_pArea->InComponentScreenPt(&point);
				if(pImage->GetMask( point.x, point.y))
					return TRUE;
			}
		}
	}
#else
	CRect rect(m_rcAREA);

	rect.OffsetRect(m_rc.TopLeft());
	ComponentToScreen(&rect);

	if(rect.PtInRect(pt))
		return TRUE;
#endif

	return CTClientUIBase::HitTest(pt);
}
#ifdef NEW_IF
BOOL CTMinimapDlg::HitTestArea( CPoint pt)
{
	LPIMAGESET pImageset = m_pArea->m_pEnable;

	if( pImageset )
	{
		pImageset->m_dwCurTick = m_pArea->m_dwTotalTick % pImageset->m_dwTotalTick;
		CD3DImage *pImage = pImageset->GetImage();

		if( pImage )
		{
			CPoint point = pt;

			m_pArea->InComponentScreenPt(&point);
			if(pImage->GetMask( point.x, point.y) && !CTClientUIBase::HitTest(pt))
				return TRUE;
		}
	}

	return FALSE;
}
#endif

void CTMinimapDlg::ShowComponent( BOOL bVisible)
{
	CTClientUIBase::ShowComponent(bVisible);

	if(m_bVisible)
		ResetVisible();
}

void CTMinimapDlg::RenderTMINIMAP()
{
	if(IsVisible())
	{
#ifdef NEW_IF
		int nRng;
		int nPos;

		m_pTZOOMSCROLL->GetScrollPos(nRng, nPos);
		m_pTMAP->m_fTSCALE = TMINIMAP_INIT_SCALE / powf(TMINIMAP_SCALE_FACTOR, (FLOAT)nPos);

		CRect rect(
			0, 0,
			TMINIMAPTEX_SIZE,
			TMINIMAPTEX_SIZE);

		rect.OffsetRect(
			m_rc.left + m_rcAREA.left - (160 - m_rcAREA.Width()) / 2,
			m_rc.top + m_rcAREA.top - (160 - m_rcAREA.Height()) / 2);
		TNLVERTEX vRECT[8] = {
			{ FLOAT(rect.left-4+50.916f), FLOAT(rect.top-2), 0.5f, 1.0f, 0xFFFFFFFF, 0.293f, 0.0f},//
			{ FLOAT(rect.left-4+122.916f), FLOAT(rect.top-2), 0.5f, 1.0f, 0xFFFFFFFF, 0.707f, 0.0f},//
			{ FLOAT(rect.left-4+173.832f), FLOAT(rect.top-2+50.916f), 0.5f, 1.0f, 0xFFFFFFFF, 1.0f, 0.293f},
			{ FLOAT(rect.left-4+173.832f), FLOAT(rect.top-2+122.916f), 0.5f, 1.0f, 0xFFFFFFFF, 1.0f, 0.707f},
			{ FLOAT(rect.left-4+122.916f), FLOAT(rect.top-2+173.832f), 0.5f, 1.0f, 0xFFFFFFFF, 0.707f, 1.0f},//
			{ FLOAT(rect.left-4+50.916f), FLOAT(rect.top-2+173.832f), 0.5f, 1.0f, 0xFFFFFFFF, 0.293f, 1.0f},//
			{ FLOAT(rect.left-4), FLOAT(rect.top-2+122.916f), 0.5f, 1.0f, 0xFFFFFFFF, 0.0f, 0.707f},
			{ FLOAT(rect.left-4), FLOAT(rect.top-2+50.916f), 0.5f, 1.0f, 0xFFFFFFFF, 0.0f, 0.293f}};
			FLOAT fMIP = 0.0f;

			m_pDevice->m_pDevice->SetSamplerState( 0, D3DSAMP_MIPMAPLODBIAS, *((LPDWORD) &fMIP));
			m_pDevice->m_pDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_ANISOTROPIC);
			m_pDevice->m_pDevice->SetTexture( 0, m_pTMINIMAP);
			m_pDevice->m_pDevice->SetFVF(T3DFVF_TNLVERTEX);

			m_pDevice->m_pDevice->DrawPrimitiveUP(
				D3DPT_TRIANGLEFAN,
				6, vRECT, sizeof(TNLVERTEX));
#else
		int nRng;
		int nPos;

		m_pTZOOMSCROLL->GetScrollPos(nRng, nPos);
		m_pTMAP->m_fTSCALE = TMINIMAP_INIT_SCALE / powf(TMINIMAP_SCALE_FACTOR, (FLOAT)nPos);

		CRect rect(
			0, 0,
			TMINIMAPTEX_SIZE,
			TMINIMAPTEX_SIZE);

		rect.OffsetRect(
			m_rc.left + m_rcAREA.left - (TMINIMAPTEX_SIZE - m_rcAREA.Width()) / 2,
			m_rc.top + m_rcAREA.top - (TMINIMAPTEX_SIZE - m_rcAREA.Height()) / 2);

		TNLVERTEX vRECT[4] = {
			{ FLOAT(rect.left), FLOAT(rect.top), 0.5f, 1.0f, 0xFFFFFFFF, 0.0f, 0.0f},
			{ FLOAT(rect.right), FLOAT(rect.top), 0.5f, 1.0f, 0xFFFFFFFF, 1.0f, 0.0f},
			{ FLOAT(rect.left), FLOAT(rect.bottom), 0.5f, 1.0f, 0xFFFFFFFF, 0.0f, 1.0f},
			{ FLOAT(rect.right), FLOAT(rect.bottom), 0.5f, 1.0f, 0xFFFFFFFF, 1.0f, 1.0f}};
			FLOAT fMIP = 0.0f;

			m_pDevice->m_pDevice->SetSamplerState( 0, D3DSAMP_MIPMAPLODBIAS, *((LPDWORD) &fMIP));
			m_pDevice->m_pDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_ANISOTROPIC);
			m_pDevice->m_pDevice->SetTexture( 0, m_pTMINIMAP);
			m_pDevice->m_pDevice->SetFVF(T3DFVF_TNLVERTEX);

			m_pDevice->m_pDevice->DrawPrimitiveUP(
				D3DPT_TRIANGLESTRIP,
				2, vRECT, sizeof(TNLVERTEX));
#endif
	}
}

HRESULT CTMinimapDlg::Render( DWORD dwTickCount)
{
	CTClientChar* pMainChar = CTClientGame::GetInstance()->GetMainChar();

#ifdef NEW_IF
	CTPotionPannel* pPotions = static_cast<CTPotionPannel*>(CTClientGame::GetInstance()->m_vTFRAME[TFRAME_POTIONS]); //VALLAH
	m_bMinimize = pPotions->m_bIsHidden;

	if(m_pArea->IsVisible())
		m_pArea->ShowComponent(FALSE);
#endif

	if( !pMainChar->IsFlashed() )
	{
		if( m_bVisible && !m_bMinimize )
		{
			m_pTMAP->ResetTMINIMAP(
				m_pTMINIMAP,
				m_pTMON,
				m_pTRSCS,
				m_pHost,
				m_pDevice,
				m_pCAM);
			RenderTMINIMAP();
		}

		return CTClientUIBase::Render(dwTickCount);
	}

	return S_OK;

	return CTClientUIBase::Render(dwTickCount);
}
