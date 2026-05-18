#pragma once


class CTMinimapDlg : public CTClientUIBase
{
public:
	static LPDIRECT3DTEXTURE9 m_pTMINIMAP;
	static LPDIRECT3DTEXTURE9 m_pTMASK;

public:
	TComponent *m_vTCOMP[TMINIMAPCOMP_COUNT];
#ifdef NEW_IF
	TComponent *m_pArea;
#else
	TComponent *m_pTITLE;
	TComponent *m_pTOPFRAME;
	TComponent *m_pMINMAXBTN;
#endif
	CD3DDevice *m_pDevice;

	CRect m_rcAREA;

	CTClientChar *m_pHost;
	LPMAPMONSTER m_pTMON;
	CTRSCSDlg *m_pTRSCS;

	CTClientMAP *m_pTMAP;
	CTClientCAM *m_pCAM;


	TScroll *m_pTZOOMSCROLL;
	BYTE m_bMinimize;

public:
	void RenderTMINIMAP();
	void ResetVisible();

public:
	virtual BOOL CanWithItemUI();
	virtual BOOL DoMouseWheel( UINT nFlags, short zDelta, CPoint pt);
	virtual BOOL HitTest( CPoint pt);
	virtual void ShowComponent( BOOL bVisible = TRUE);
	virtual HRESULT Render( DWORD dwTickCount);
#ifdef NEW_IF
	BOOL HitTestArea( CPoint pt);
#endif

public:
	CTMinimapDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc);
	virtual ~CTMinimapDlg();
};
