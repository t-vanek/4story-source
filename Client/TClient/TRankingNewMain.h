#pragma once


class CTRankingNewMain : public CTFrameGroupBase
{

public:
	virtual void ShowComponent( BOOL bVisible );
	virtual ITDetailInfoPtr GetTInfoKey( const CPoint& point );

public :
	CTRankingNewMain( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual	~CTRankingNewMain();
};