#pragma once

class CTBowRespawnDlg : public CTClientUIBase
{
public:
	TButton*	m_pTime;
	TComponent* m_pPrice;
public:
	void SetTime(DWORD dwTick);
public:
	virtual BOOL CanWithItemUI();
	virtual void ShowComponent(BOOL bVisible);
public:
	CTBowRespawnDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CTBowRespawnDlg();
};