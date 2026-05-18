#pragma once

class CTBowBPTradeDlg : public CTClientUIBase
{
public:
	TComponent* m_pMin; 
	TComponent* m_pMax;

	TComponent* m_pTotalMedals;
	TComponent* m_pMedalCost;

	TComponent* m_pTradeDesc;

	TButton*	m_pBuy;
	TScroll*	m_pRange;
public:
	static DWORD m_dwMedals;
	static DWORD m_dwBP;
public:
	void UpdateScroll();
	virtual HRESULT Render(DWORD dwTickCount);
public:
	CTBowBPTradeDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CTBowBPTradeDlg();
};