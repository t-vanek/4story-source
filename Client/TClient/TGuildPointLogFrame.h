#pragma once

class CTGuildPointLogFrame : public CTClientUIBase
{
public:
	CTGuildPointLogFrame( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc);
	virtual ~CTGuildPointLogFrame();

public:
	TList* m_pList;
};