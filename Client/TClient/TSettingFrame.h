#pragma once

class CTSettingFrame : public CTFrameGroupBase
{
public:
	CTSettingFrame(TComponent* pParent, FRAMEDESC_SHAREDPTR pDesc, DWORD dwInnerPosID);
	virtual ~CTSettingFrame();
	virtual void OnLButtonDown(UINT nFlags, CPoint pt);
	void UpdateUI();
	TButton* m_pBTN_SHOW_CAUTION15;
};