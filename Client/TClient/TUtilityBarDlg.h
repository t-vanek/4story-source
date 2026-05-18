#pragma once

class CTUtilityBarDlg : public CTClientUIBase
{
private :

public :
	TButton* m_pButEx;
	TButton* m_pButIdx[9];
	TComponent* m_pLock;

	BYTE bShownBar;
	BYTE bShownAll;
	BOOL CanWithItemUI();
	BYTE bShownFade;

	CTUtilityBarDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CTUtilityBarDlg();
	HRESULT Render(DWORD dwTickCount);
	void OnLButtonUp(UINT nFlags, CPoint pt);
};
