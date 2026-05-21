#pragma once


class CTMapSession : public CSession
{
public:
	BYTE m_bCanDelete;
	BYTE m_bCheckedSession;

public:
	CTMapSession();
	virtual ~CTMapSession();
};
