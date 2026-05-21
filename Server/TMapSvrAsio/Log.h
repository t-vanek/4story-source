#pragma once

#include "StdAfx.h"

class CLog
{
private:
	CString m_name;
	FILE* m_pFile;

public:
	CLog(LPTSTR szName);
	~CLog();

	BOOL Open();

	void WriteMessage(CString message);
	void WritePacket(CPacket* pPacket);
};