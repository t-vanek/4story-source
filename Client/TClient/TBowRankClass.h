#pragma once

class CTBowRank
{
public:
	CTBowRank();
	virtual ~CTBowRank();

public:
	DWORD m_dwSP;
	DWORD m_dwBB;
	DWORD m_dwCharID;
	CString m_strName;
	BYTE m_bCountry;
	CString m_strGuild;
};
