#pragma once
typedef enum BRPANNELS_COMPONENTS
{
	COMP_GAUGE,
	COMP_HEARTPH,
	COMP_LIFES,
	COMP_CLASS,
	COMP_NAME,
	COMP_BUTTON,
	COMP_DANGER,
	COMP_DANGER_PH,
	COMP_COUNT
};

class CBRPannels : public CTClientUIBase
{
public:
	TComponent* m_pName    [BR_TEAMMATE_MAX_COUNT(BR_3V3)];
	TGauge*     m_pGauge   [BR_TEAMMATE_MAX_COUNT(BR_3V3)];
	TComponent* m_pHeartPH [BR_TEAMMATE_MAX_COUNT(BR_3V3)]; //heart placeholder
	TComponent* m_pLifes   [BR_TEAMMATE_MAX_COUNT(BR_3V3)];
	TButton*    m_pButton  [BR_TEAMMATE_MAX_COUNT(BR_3V3)];
	TComponent* m_pDanger  [BR_TEAMMATE_MAX_COUNT(BR_3V3)];
	TComponent* m_pDangerPH[BR_TEAMMATE_MAX_COUNT(BR_3V3)]; //danger placeholder

public:
	INT m_nLastSel;

	INT GetLastSel() { return m_nLastSel; }
	void BuildMenu();
public:
	CBRPannels( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CBRPannels();
	virtual BOOL	CanWithItemUI();
	virtual void ShowComponent(BOOL bVisible = TRUE);

	void UpdatePannels();
	virtual void OnLButtonUp(UINT nFlags, CPoint pt);
	virtual void OnRButtonUp(UINT nFlags, CPoint pt);
	virtual HRESULT Render(DWORD dwTickCount);
};