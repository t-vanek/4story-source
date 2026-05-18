#pragma once

class CTCustomCloakDlg : public CTClientUIBase
{
public:
	TList*		m_pList;
	TComponent *m_p3D;
	WORD		m_wSelectedCloak;
	
	virtual void ShowComponent( BOOL bVisible = TRUE);
	virtual HRESULT Render(DWORD dwTickCount);	

	virtual void ClearCloaks();
	virtual void AddCloak(WORD wID,__int64 dDateTime);
	virtual void OnLButtonDown( UINT nFlags, CPoint pt );

	void EnableTLIGHT(
		CD3DCamera *pCamera,
		BYTE bENABLE);
	void ResetData(
		CTClientChar *pCHAR,
		CTachyonRes *pRES);
	void ResetCHAR();

public:
	CD3DLight m_vLIGHT[TLIGHT_COUNT];
	CTClientChar *m_pCHAR;
	CD3DDevice *m_pDevice;
	CD3DCamera m_vCamera;
	
public:
	CTCustomCloakDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CTCustomCloakDlg();
};
