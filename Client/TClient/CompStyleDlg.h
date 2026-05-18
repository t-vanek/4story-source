#pragma once

class CCompStyleDlg : public CTClientUIBase
{
public:
	TImageList* m_pFaceIcon;
	TComponent* m_pFinishTxt;
	TImageList* m_pTargetIcon;

	TButton* m_pStart;
	TGauge* m_pGauge;
public:
	BYTE m_bFromSlot;
	WORD m_wToMonID;

	BYTE m_bProgress;
	BYTE m_bFinished;

	BYTE m_bInvenID;
	BYTE m_bItemID;
public:
	BYTE InitData(BYTE bSlot, WORD wToMonID, BYTE bInvenID, BYTE bItemID);
public:
	virtual HRESULT Render(DWORD dwTickCount);
	virtual BOOL CanWithItemUI();
	virtual void OnLButtonUp(UINT nFlags, CPoint pt);
	virtual void ShowComponent(BOOL bVisible);
public:
	CCompStyleDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CCompStyleDlg();
};