#pragma once


class CTCompanionMiniUI : public CTClientUIBase
{

public:
	TImageList* m_pIcon;
	TImageList* m_pItems[2];
	TGauge*     m_pCooldowns[2];
	TComponent* m_pPets[5];

public:
	HRESULT Render(DWORD dwTickCount);
public:
	CTCompanionMiniUI( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CTCompanionMiniUI();
};