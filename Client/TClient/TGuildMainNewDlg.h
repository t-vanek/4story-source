#pragma once


class CTGuildMainNew : public CTFrameGroupBase
{

public:
	//virtual void OnLButtonUp(UINT nFlags, CPoint pt);
	virtual void OnLButtonDown(UINT nFlags, CPoint pt);
	virtual void OnKeyDown(UINT nChar, int nRepCnt, UINT nFlags);
	virtual ITDetailInfoPtr GetTInfoKey( const CPoint& point );
	virtual void ShowComponent(BOOL bVisible = TRUE);
	virtual BYTE OnBeginDrag(LPTDRAG pDRAG, CPoint point);
	//virtual void ResetPosition();
public:
	TEdit*					GetCurEdit();

public :
	CTGuildMainNew( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual	~CTGuildMainNew();
};