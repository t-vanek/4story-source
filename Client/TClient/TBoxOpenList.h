#pragma once
struct Reward
{
	CTClientItem *m_pTITEM;



};

class CTBoxOpenListDlg : public CTClientUIBase
{
private :

	TComponent* m_pItemName[7];
	TComponent* m_pHighlight[7];
	TImageList* m_pItemIcon[7];
	TScroll* m_pTSCROLL;

public :
	std::vector<Reward> Odmeny;
	INT m_nListTop;

	CTBoxOpenListDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CTBoxOpenListDlg();
	HRESULT Render(DWORD dwTickCount);
	void Release();
	void SetSession(WORD m_wBoxID, BYTE m_bInvenID, BYTE m_bSlotID);
	void UpdateScrollPosition();

	void ResetPosition();
	void AddItems(CTClientItem * pTITEM);
	ITDetailInfoPtr GetTInfoKey( const CPoint& point );

	virtual void OnNotify( DWORD from, WORD msg, LPVOID param );
	virtual BOOL DoMouseWheel( UINT nFlags, short zDelta, CPoint pt);
};
