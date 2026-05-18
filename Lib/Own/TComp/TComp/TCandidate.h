#pragma once
#include "TFrame.h"

class TCandidate : public TFrame
{
public:
	TCandidate(TComponent* pParent, FRAMEDESC_SHAREDPTR pDesc)
		: TFrame(pParent, pDesc)
	{

	};

	TCandidate(TComponent* pParent, FRAMEDESC_WEAKPTR pDesc)
		: TCandidate(pParent, pDesc.lock())
	{

	}

	TCandidate(TComponent* pParent, const TFrame& rSrcCandidate)
		: TFrame(pParent, rSrcCandidate)
	{

	};

	virtual ~TCandidate()
	{}

	virtual void ResetCandidateChar()=0;
	virtual void SetCandidateChar( std::vector<CString>& vString )=0;
	virtual void SetSelection( INT nIndex )=0;
	virtual void MoveComponentAdjustText( CRect& )=0;
};
