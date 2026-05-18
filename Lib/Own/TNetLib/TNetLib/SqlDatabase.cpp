// SqlDatabase.cpp: implementation of the CSqlDatabase class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CSqlDatabase::CSqlDatabase()
{
	Clear();
}

CSqlDatabase::~CSqlDatabase()
{
	Close();
}

BOOL CSqlDatabase::Open(LPCTSTR lpszDsn,
						LPCTSTR lpszUser,
						LPCTSTR lpszPasswd)
{
	SQLRETURN sqlret;

	if (!InitializeCriticalSectionAndSpinCount(&m_csExecution, 4000))
		return FALSE;
	m_bCsInitialized = TRUE;

	sqlret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_henv);
	if( TSQL_FAIL( sqlret ) )
	{
		// Bug fix: previous code returned FALSE here without calling Close(),
		// leaking the critical section and (in driver-internal state) any
		// partially-allocated ENV handle. Funnel through Close() like every
		// other failure path below.
		Close();
		return FALSE;
	}

	sqlret = SQLSetEnvAttr(m_henv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
	if( TSQL_FAIL( sqlret ) )
	{
		Close();
		return FALSE;
	}

	sqlret = SQLAllocHandle(SQL_HANDLE_DBC, m_henv, &m_hdbc);
	if( TSQL_FAIL( sqlret ) )
	{
		Close();
		return FALSE;
	}

	sqlret = SQLConnect( m_hdbc,
		(SQLCHAR*)lpszDsn, SQL_NTS,
		(SQLCHAR*)lpszUser, SQL_NTS,
		(SQLCHAR*)lpszPasswd, SQL_NTS);
	if( TSQL_FAIL( sqlret ) )
	{
		Close();
		return FALSE;
	}

	m_bOpen = TRUE;

	return TRUE;
}

void CSqlDatabase::Close()
{
	ClearQuery();

	// Bug fix: DeleteCriticalSection on uninitialized memory is UB. The
	// destructor calls Close() unconditionally, so a default-constructed
	// CSqlDatabase that never had Open() called (or Open failed before
	// the InitializeCriticalSectionAndSpinCount line) used to corrupt
	// memory here.
	if( m_bCsInitialized )
	{
		DeleteCriticalSection(&m_csExecution);
		m_bCsInitialized = FALSE;
	}

	if( IsOpen() )
		SQLDisconnect(m_hdbc );

	if( SQL_NULL_HANDLE != m_hdbc )
		SQLFreeHandle( SQL_HANDLE_DBC, m_hdbc);

	if( SQL_NULL_HANDLE != m_henv )
		SQLFreeHandle( SQL_HANDLE_ENV, m_henv);

	Clear();
}

void CSqlDatabase::Clear()
{
	m_bOpen = FALSE;
	m_bCsInitialized = FALSE;
	m_hdbc = m_henv = SQL_NULL_HANDLE;
}

BOOL CSqlDatabase::IsOpen()
{
	return m_bOpen;
}


SQLHDBC CSqlDatabase::HDBC()
{
	return m_hdbc;
}

BOOL CSqlDatabase::AddQuery(CSqlQuery *pQuery, string strKey)
{
	QUERY_MAP::iterator it;

	it = m_mapQuery.find(strKey);
	if( it != m_mapQuery.end()) return FALSE;

	m_mapQuery.insert(QUERY_MAP::value_type(strKey, pQuery));
	return TRUE;		
}

CSqlQuery * CSqlDatabase::DelQuery(string strKey)
{
	CSqlQuery *pQuery = NULL;

	QUERY_MAP::iterator it = m_mapQuery.find(strKey);
	if( it != m_mapQuery.end())
	{
		pQuery =(*it).second;
		m_mapQuery.erase(it);
	}

	return pQuery;
}

CSqlQuery * CSqlDatabase::GetQuery(string strKey)
{
	CSqlQuery *pQuery = NULL;

	QUERY_MAP::iterator it = m_mapQuery.find(strKey);
	if( it != m_mapQuery.end()) pQuery =(*it).second;

	return pQuery;
}

void CSqlDatabase::ClearQuery()
{
	QUERY_MAP::iterator it;

	for( it = m_mapQuery.begin(); it != m_mapQuery.end(); it++)
	{
		(*it).second->ResetKey();
		delete (*it).second;
	}
	m_mapQuery.clear();
}
