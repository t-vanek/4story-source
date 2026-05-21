#pragma once
#include "trecallmon.h"

class CTCompanion :
	public CTRecallMon
{

public:
	DWORD m_dwMonID;
	BYTE m_bEffect;
	BYTE m_bCompanionSlot;

public:
	CTCompanion(void);
	~CTCompanion(void);

	virtual void Recover(DWORD dwTick);
	virtual void OnDie(
		DWORD dwAttackID,
		BYTE bObjectType, 
		WORD wTempMonID);
};
