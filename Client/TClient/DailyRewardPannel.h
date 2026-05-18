#pragma once

#define MAX_DAILY_REWARD (5)

class CDailyRewardPannel : public CTClientUIBase
{
public:
	TImageList* m_pItem[MAX_DAILY_REWARD];
	TGauge* m_pGauge[MAX_DAILY_REWARD];
	TComponent* m_pChoosen;

public:
public:
	CDailyRewardPannel( TComponent *pParent, LP_FRAMEDESC pDesc );
	virtual ~CDailyRewardPannel();
};
