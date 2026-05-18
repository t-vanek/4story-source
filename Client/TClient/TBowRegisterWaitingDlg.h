#pragma once


class CTBowRegisterWaitingDlg : public CTClientUIBase
{
public:
	TImageList* m_pMinute[2];
	TImageList* m_pSecond[2];
public:
	void SetTime(BYTE bMin, BYTE bSec);
public:
	virtual BOOL CanWithItemUI();
	virtual void ShowComponent(BOOL bVisible);
public:
	CTBowRegisterWaitingDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CTBowRegisterWaitingDlg();
};