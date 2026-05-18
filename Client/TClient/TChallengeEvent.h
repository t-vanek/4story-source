#pragma once

class CTChallengeEvent : public CTClientUIBase
{
public:
	CTChallengeEvent(TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc);
	virtual ~CTChallengeEvent(void);
	virtual HRESULT Render(DWORD dwTickCount);

public:
	DWORD m_dwGaugeTick;
	TComponent* m_pMSG_UP;
	TGauge* m_pGAUGE;
	TList* m_pLIST;
};