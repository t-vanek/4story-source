#pragma once

class CTBodRank
{
public:
	CTBodRank();
	virtual ~CTBodRank();

public:
	DWORD wTotalPoints;
	DWORD wBB;
	DWORD dwCharID;
	CString strName;
	BYTE bCountry;
	CString strGuild;
};
