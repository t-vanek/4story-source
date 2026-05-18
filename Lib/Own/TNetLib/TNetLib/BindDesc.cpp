// BindDesc.cpp: implementation of the CBindDesc class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CBindDesc::CBindDesc()
{
	// Bug fix: m_size / m_type were left uninitialized. A subsequent
	// MAlloc() (zero-arg form) calls MAlloc(m_size), passing a garbage
	// size to malloc — at best a wild allocation, at worst a crash. Zero
	// them out so a default-constructed descriptor is at least safe to
	// destroy and to MAlloc(explicit_size) into.
	m_ptr = NULL;
	m_type = 0;
	m_size = 0;
}

CBindDesc::CBindDesc( SQLSMALLINT type, int size)
{
	m_ptr = NULL;
	m_type = type;
	m_size = size;
}

CBindDesc::~CBindDesc()
{
	Free();
}

void CBindDesc::MAlloc()
{
	MAlloc(m_size);
}

void CBindDesc::MAlloc(DWORD cb)
{
	Free();
	m_ptr = malloc(cb);
}

void CBindDesc::Free()
{
	if(m_ptr)	
		free(m_ptr);
	m_ptr = NULL;
}
