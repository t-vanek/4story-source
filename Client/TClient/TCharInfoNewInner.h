#pragma once


class CTCharInfoNewDlg : public CTFrameGroupBase
{

public:
	virtual void ShowComponent( BOOL bVisible );
	virtual ITDetailInfoPtr GetTInfoKey( const CPoint& point );
	virtual void OnLButtonDown(UINT nFlags, CPoint pt);
	virtual HRESULT Render(DWORD dwTickCount);
	virtual BYTE OnBeginDrag( LPTDRAG pDRAG, CPoint point);
	virtual BOOL DoMouseWheel( UINT nFlags, short zDelta, CPoint pt);
	virtual TDROPINFO OnDrop( CPoint point);
	virtual BOOL GetTChatInfo(const CPoint& point, TCHATINFO& outInfo );

	void ResetTabs();

	BYTE m_bTab;
	TButton* m_pTabs[4];
	CTCharNewDlg* m_pFRAME;
	TFrame* m_pFRAMES[3];
	CPoint m_ptFRAMEPOS;

public :
	CTCharInfoNewDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc, TCMLParser *pParser, CD3DDevice *pDevice, WORD wFrameID, CTClientChar* pHost);
	static BOOL SortByTime( LPTLATESTPVPINFO pLeft, LPTLATESTPVPINFO pRight );

	virtual	~CTCharInfoNewDlg();
};