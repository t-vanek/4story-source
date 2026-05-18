#pragma once
class CBattleRankProgressDlg : public CTClientUIBase
{
public:
	TComponent* m_pRankPoint;
	TComponent* m_pMaxRankPoint;
	TComponent* m_pPlaceHolder[2];

	TImageList* m_pNextRank;

	TGauge* m_pProgress;
public:
	void SetInfo(DWORD dwCurPoint);
	virtual void OnLButtonUp(UINT nFlags, CPoint pt);
public:
	CBattleRankProgressDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc, CTMainUI* pMainUI );
	virtual ~CBattleRankProgressDlg();
	virtual BOOL	CanWithItemUI();
};