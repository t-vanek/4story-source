#pragma once

class CTBowGRank
{
public:
	CTBowGRank();
	virtual ~CTBowGRank();

public:
	DWORD m_dwID;
	CString m_strName;
	BYTE m_bCountry;
	DWORD m_dwBB;
	DWORD m_dwSP;
};
