#pragma once

class CTCompanionAttrs : public ITInnerFrame
{
public:
	virtual void RequestInfo()	{}
	virtual void ResetInfo()	{}
protected:


public :
	TComponent* m_pStatPText;
	TComponent* m_pStatPValue;
	TComponent*		m_pStatT[6];
	TButton*		m_pStatB[6];
	TComponent*		m_pStatV[6];

	CTCompanionAttrs( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	void OnLButtonUp(UINT nFlags, CPoint pt);
	virtual ~CTCompanionAttrs();
};