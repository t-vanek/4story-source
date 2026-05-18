#pragma once
#include "StdAfx.h"
#include <unordered_map>

class SmoothScrolling : public TScroll
{
private:
	std::vector<SmoothData> m_vComponentList;
	std::vector<SmoothData> m_vImageList;
public:
	TFrame* m_pSource;
	BYTE m_Items;
public:
	SmoothScrolling(TComponent* pParent, FRAMEDESC_SHAREDPTR pDesc);
	~SmoothScrolling();
	void Setup(CRect rArea, DWORD* All, BYTE bDynObjects, CTClientUIBase * pSource);
	BOOL DoMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	void OnNotify(DWORD from, WORD msg, LPVOID param);
	void AddItem(BYTE bID, BYTE bIndex, WORD wImage);
	void AddItem(BYTE bID, BYTE bIndex, CString strText);
};
