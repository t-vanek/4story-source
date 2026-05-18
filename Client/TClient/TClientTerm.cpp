#include "StdAfx.h"


CTClientTerm::CTClientTerm()
{
	m_pTTERM = NULL;

	m_bStatus = 0;
	m_bCount = 0;
}

CTClientTerm::~CTClientTerm()
{
}

BYTE CTClientTerm::GetResult()
{
	BYTE bMaxCount = m_pTTERM ? m_pTTERM->m_bCount : 1;
	return m_bCount < bMaxCount ? TTERMRESULT_INCOMPLETE : TTERMRESULT_COMPLETE;
}
