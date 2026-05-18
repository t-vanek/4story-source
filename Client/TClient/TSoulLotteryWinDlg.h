#pragma once

class CTLotWinDlg : public CTClientUIBase
{
public:
	TComponent* m_pGold;
	TComponent* m_pSilver;
	TComponent* m_pCron;
	TComponent* m_pSText;
	TButton* m_pOK;
public:
	void SetReward(DWORD dwMoney);
	virtual void OnLButtonUp(UINT nFlags, CPoint pt);
public:
	CTLotWinDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CTLotWinDlg();
	virtual BOOL	CanWithItemUI();
};