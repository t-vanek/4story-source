#pragma once

class CTCompanionStats : public ITInnerFrame
{
public:
	virtual void RequestInfo()	{}
	virtual void ResetInfo()	{}

	TComponent*		m_pAttr[5];
	TComponent*		m_pAttrV[5];
protected:

public :
	CTCompanionStats( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CTCompanionStats();
};