#include "StdAfx.h"

#include "Log.h"

CLog::CLog(LPTSTR szName)
{
	if (szName)
		m_name = szName;
	else
		m_name = _T("Unnamed");
}

CLog::~CLog()
{
	if (m_pFile)
	{
		fclose(m_pFile);
		m_pFile = NULL;
	}
}

BOOL CLog::Open()
{
	CString strFileName;

	time_t t = time(NULL);
	tm* tm = localtime(&t);

	strFileName.Format(_T("C:\\logs\\%d-%d-%d %d-%d-%d %s.log"), 
		tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, 
		tm->tm_hour, tm->tm_min, tm->tm_sec, (LPCTSTR)m_name);

	m_pFile = fopen((LPCTSTR)strFileName, "a");

	return m_pFile != NULL;
}

void CLog::WriteMessage(CString message)
{
	if (m_pFile)
	{
		fputs((LPCTSTR)message, m_pFile);
		fputs(_T("\r\n"), m_pFile);
		fflush(m_pFile);
	}
}

void CLog::WritePacket(CPacket* pPacket)
{
	if (m_pFile && pPacket)
	{
		if (pPacket->m_pHeader)
		{
			CString output;

			output.Format(_T("PACKET %hx SIZE=%d\r\n"), pPacket->m_pHeader->m_wID, pPacket->m_pHeader->m_wSize);
			fputs((LPCTSTR)output, m_pFile);

			WORD wSize = pPacket->m_pHeader->m_wSize;
			LPBYTE pBuffer = pPacket->GetBuffer();

			for (WORD i = 0; i < wSize; i++)
			{
				output.Format(_T("%02X"), pBuffer[i]);
				fputs((LPCTSTR)output, m_pFile);

				if (i != 0 && i % 128 == 0)
					fputs(_T("\r\n"), m_pFile);
			}

			fputs(_T("\r\n"), m_pFile);

		}
		else
			fputs(_T("PACKET HEADER NULL\r\n"), m_pFile);

		fflush(m_pFile);
	}
}

