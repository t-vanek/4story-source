#pragma once

class CTBodGRank
{
public:
	CTBodGRank();
	virtual ~CTBodGRank();

public:
	DWORD m_dwID;
	CString m_strName;
	BYTE m_bCountry;
	DWORD m_dwBB;
	DWORD m_dwTP;
};
