#pragma once


class CTBattleInvenDlg : public CTInvenDlg
{
public:
	TButton* m_pClose;
public:
	virtual ITDetailInfoPtr GetTInfoKey( const CPoint& point );
	virtual BOOL GetTChatInfo( const CPoint& point, TCHATINFO& outInfo );
	virtual BYTE OnBeginDrag( LPTDRAG pDRAG, CPoint point);
	virtual TDROPINFO OnDrop( CPoint point);
	virtual HRESULT Render(DWORD dwTickCount);
	virtual void MoveComponent(CPoint pt);
	virtual void OnLButtonUp(UINT nFlags, CPoint pt);
public:
	CTBattleInvenDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc, TCMLParser *pParser);
	virtual ~CTBattleInvenDlg();
};
