#pragma once


class CTBowRegisterDlg : public CTClientUIBase
{
public:
	TComponent* m_pText;
	TButton*    m_pButton;
public:
	BYTE m_bType;
public:
	void SetType(BYTE bType);
	BYTE GetType() { return m_bType; }
public:
	virtual BOOL CanWithItemUI();
	virtual void ShowComponent(BOOL bVisible);
public:
	CTBowRegisterDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CTBowRegisterDlg();
};