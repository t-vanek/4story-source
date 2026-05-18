// Packet.cpp: implementation of the CPacket class.
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"

//COverlappedEx
COverlappedEx::COverlappedEx()
{
	memset( &m_ov, 0, sizeof(OVERLAPPED));

	m_bTYPE = TOV_SSN_RECV;
	m_pOBJ = NULL;
}

COverlappedEx::~COverlappedEx()
{
}


//CPacket
CPacket::CPacket()
{
	m_dwBufferSize = 0;
	m_pHeader = NULL;
	m_pBuf = NULL;
	m_bType = PACKET_SEND;

	ExpandIoBuffer(-1);
	Clear();
}

CPacket::~CPacket()
{
	if( m_pBuf && m_dwBufferSize )
		delete[] m_pBuf;
}

void CPacket::Encrypt(INT64 key)
{
	if(!m_pHeader) return;

	INT64 llCheckSum = 0;
	DWORD dwDataSize = m_pHeader->m_wSize - PACKET_HEADER_SIZE;
	INT64 crc = 0;

	DWORD body = dwDataSize / sizeof(INT64);
	DWORD left = dwDataSize % sizeof(INT64);

	PINT64 lpBody = (PINT64)(m_pBuf + PACKET_HEADER_SIZE);
	for(DWORD i = 0; i < body; i++)
	{
		llCheckSum ^= lpBody[i];
		lpBody[i] ^= key;  
	}

	LPBYTE lpLeft = (LPBYTE)&lpBody[body];
	LPBYTE pchKey = (LPBYTE)&key;
	for(DWORD i = 0; i < left; i++)
	{
		llCheckSum ^=(BYTE)lpLeft[i]; 
		lpLeft[i] ^= (BYTE)pchKey[i];
		crc = (((crc >> 4) & 0x0FFD) ^ key);
		llCheckSum += crc;
	}

	m_pHeader->m_llChkSUM = llCheckSum;
}

void CPacket::EncryptHeader(INT64 key)
{
	if(!m_pHeader) return;

	LPBYTE lpHeader = (LPBYTE)((LPBYTE)m_pHeader+sizeof(WORD));
	WORD wID = m_pHeader->m_wID;
	// Bug fix: loop counter was BYTE — the comparison i<14 was correct only
	// because PACKET_HEADER_SIZE is 16 today. If the header struct ever
	// grows past 255 bytes, the comparison silently wraps and the loop
	// runs forever, corrupting memory past the header. DWORD is wide
	// enough for any plausible header size.
	const DWORD dwSpan = (DWORD)(PACKET_HEADER_SIZE - sizeof(WORD));
	for(DWORD i=0; i<dwSpan; i++)
	{
		if(i<2)
			lpHeader[i] ^= (BYTE)(key + m_pHeader->m_wSize + i);
		else
			lpHeader[i] ^= (BYTE)(key + wID + i);
	}
}

BOOL CPacket::Decrypt(INT64 key)
{
	if(!m_pHeader) return FALSE;


	INT64 llCheckSum1 = 0;
	INT64 llCheckSum2 = m_pHeader->m_llChkSUM;
	DWORD dwDataSize = m_pHeader->m_wSize - PACKET_HEADER_SIZE;
	INT64 crc = 0;

	DWORD body = dwDataSize / sizeof(INT64);
	DWORD left = dwDataSize % sizeof(INT64);

	PINT64 lpBody = (PINT64)(m_pBuf + PACKET_HEADER_SIZE);
	for(DWORD i = 0; i < body; i++)
	{
		lpBody[i] ^= key;
		llCheckSum1 ^= lpBody[i];
	}

	LPBYTE lpLeft = (LPBYTE)&lpBody[body];
	LPBYTE pchKey = (LPBYTE)&key;
	for(DWORD i = 0; i < left; i++)
	{
		lpLeft[i] ^= (BYTE)pchKey[i];
		llCheckSum1 ^= (BYTE)lpLeft[i];
		crc = (((crc >> 4) & 0x0FFD) ^ key);
		llCheckSum1 += crc;
	}

	if( llCheckSum1 != llCheckSum2 ) return FALSE;

	return TRUE;
}

void CPacket::DecryptHeader(INT64 key)
{
	if(!m_pHeader) return;

	LPBYTE lpHeader = (LPBYTE)((LPBYTE)m_pHeader+sizeof(WORD));
	// See EncryptHeader for rationale on the DWORD counter widening.
	const DWORD dwSpan = (DWORD)(PACKET_HEADER_SIZE - sizeof(WORD));
	for(DWORD i=0; i<dwSpan; i++)
	{
		if(i<2)
			lpHeader[i] ^= (BYTE)(key + m_pHeader->m_wSize + i);
		else
			lpHeader[i] ^= (BYTE)(key + m_pHeader->m_wID + i);
	}
}

DWORD CPacket::ExpandIoBuffer( DWORD dwNewSize)
{
	DWORD dwPrevOffset = PACKET_HEADER_SIZE;
	DWORD dwExpandSize;
	DWORD dwAddSize =0;

	LPBYTE pExpand = NULL;

	if( dwNewSize != -1 )
		dwExpandSize = dwNewSize;
	else
		dwExpandSize = m_dwBufferSize + DEF_PACKET_SIZE;

	if( dwExpandSize <= m_dwBufferSize )
		return dwAddSize;

	pExpand = new BYTE[dwExpandSize];
	memset(pExpand, 0, sizeof(BYTE) * dwExpandSize);

	if(m_pBuf)
	{
		memcpy( pExpand, m_pBuf, m_dwBufferSize);
		dwPrevOffset = DWORD(m_ptrOffset - m_pBuf);

		delete[] m_pBuf;
	}

	m_dwBufferSize = dwExpandSize;
	m_pBuf = pExpand;

	m_pHeader = (LPPACKETHEADER) pExpand;
	m_ptrOffset = m_pBuf + dwPrevOffset;

	return dwAddSize;
}

void CPacket::Clear()
{
	memset( m_pBuf, 0, m_dwBufferSize);
	m_dwReadBytes = 0;

	if(m_bType == PACKET_SEND)
		m_pHeader->m_wSize = PACKET_HEADER_SIZE;

	m_ptrOffset = m_pBuf + PACKET_HEADER_SIZE;
}

LPBYTE CPacket::GetBuffer()
{
	return m_pBuf;
}

void CPacket::Flush()
{
	DWORD dwTrashSize = GetSize();

	if( m_dwReadBytes < dwTrashSize )
	{
		Clear();
		return;
	}

	m_dwReadBytes -= dwTrashSize;
	m_ptrOffset = m_pBuf + PACKET_HEADER_SIZE;

	memcpy( m_pBuf, m_pBuf + dwTrashSize, m_dwReadBytes);
}

BOOL CPacket::IsValid()
{
	if(m_pHeader->m_wSize >= MAX_PACKET_SIZE)
		return FALSE;

	return TRUE;
}

BOOL CPacket::IsReadBufferFull()
{
	return m_dwReadBytes >= PACKET_HEADER_SIZE && GetSize() > m_dwBufferSize;
}

BOOL CPacket::IsEOF()
{
	return !CanRead(1);
}

BOOL CPacket::CanRead( DWORD length)
{
	// Bug fix: previously this function did
	//     DWORD dwReadBytes = DWORD(m_ptrOffset - m_pBuf) + length;
	// with no overflow guard. Callers reaching this from
	// operator>>(CString&) pass a signed int read from a wire packet;
	// a negative input cast to DWORD becomes ~4 GB, then
	// `offset + length` wraps modulo 2^32 to a tiny value, the
	// "is it within buffer?" check passes, and Read() then memcpys
	// 4 GB into a 0-byte heap allocation — remote heap corruption
	// from any unauthenticated peer that can frame one packet.
	// Reject lengths past the wire-format ceiling outright; that
	// fixes the wrap source.
	if( length > MAX_PACKET_SIZE )
		return FALSE;

	const ptrdiff_t offset = m_ptrOffset - m_pBuf;
	if( offset < 0 )
		return FALSE;
	if( static_cast<DWORD>(offset) > MAX_PACKET_SIZE - length )
		return FALSE; // sum would still wrap even after the length guard

	DWORD dwReadBytes = static_cast<DWORD>(offset) + length;
	WORD wPacketSize = GetSize();

	return m_dwBufferSize >= dwReadBytes && wPacketSize >= dwReadBytes;
}

BOOL CPacket::CanWrite( DWORD length)
{
	return m_dwBufferSize > GetSize() + length;
}

WORD CPacket::GetID()
{
	return m_pHeader ? m_pHeader->m_wID : 0xFFFF;
}

CPacket& CPacket::SetID( WORD wID)
{
	if(m_pHeader)
		m_pHeader->m_wID = wID;

	return *this;
}

WORD CPacket::GetSize()
{//Packet size, it does not mead the size of valid data or buffer
	return m_pHeader ? m_pHeader->m_wSize : 0xFFFF;
}

void CPacket::Read( LPVOID param, int nLength)
{
	// Bug fix: defensive guard. nLength is signed; the public Read API
	// historically passed it through to memcpy / memset with implicit
	// cast to size_t. A negative value (e.g. attacker-supplied length
	// from operator>>(CString&)) became ~16 EB on 64-bit hosts and
	// trashed adjacent memory. Treat non-positive / oversized lengths
	// as a no-op.
	if (nLength <= 0 || nLength > static_cast<int>(MAX_PACKET_SIZE))
		return;

	const DWORD dwLength = static_cast<DWORD>(nLength);
	if(CanRead(dwLength))
	{
		memcpy( param, m_ptrOffset, dwLength);
		m_ptrOffset += dwLength;
	}
	else
		memset( param, 0, dwLength);
}

void CPacket::Write( LPVOID param, int nLength)
{
	if(!IsValid())
		return;

	// Bug fix: previous code would memcpy first then cap m_wSize at
	// MAX_PACKET_SIZE (which IsValid() rejects as invalid — see :201),
	// leaving a packet with bytes written but a wSize the rest of the
	// codebase treats as corrupt. Refuse the write up-front instead, so
	// the caller has a chance to handle it (no signal here as Write
	// returns void, but at least the packet stays internally consistent).
	if((DWORD)m_pHeader->m_wSize + (DWORD)nLength >= MAX_PACKET_SIZE)
		return;

	if(!CanWrite(nLength))
		ExpandIoBuffer(m_dwBufferSize + max(nLength, DEF_PACKET_SIZE));

	memcpy( m_ptrOffset, param, nLength);
	m_ptrOffset += nLength;
	m_pHeader->m_wSize += (WORD)nLength;
}

void CPacket::Copy( CPacket *pMsg)
{
	if(!pMsg)
		return;

	DWORD dwPacketSize = pMsg->GetSize();
	if( dwPacketSize >= MAX_PACKET_SIZE )
		return;

	if( m_dwBufferSize < dwPacketSize )
		ExpandIoBuffer(dwPacketSize);

	Clear();

	memcpy( m_pBuf, pMsg->m_pBuf, dwPacketSize);
	m_pHeader->m_llChkSUM = 0;
	m_ptrOffset = m_pBuf + dwPacketSize;
}

void CPacket::CopyData(CPacket * pMsg, WORD wDeleteSize)
{
	if(!pMsg || pMsg->GetSize() < wDeleteSize + PACKET_HEADER_SIZE)
		return;

	WORD wAddSize = pMsg->GetSize() - PACKET_HEADER_SIZE - wDeleteSize;

	// Bug fix: AddData (right below) does this check via `>= MAX_PACKET_SIZE`;
	// CopyData was missing it, so a large append could wrap m_wSize (both
	// operands are WORD). Guard the same way so the wire-level invariant
	// `wSize < MAX_PACKET_SIZE` is preserved on either entry point.
	if( (DWORD)GetSize() + (DWORD)wAddSize >= MAX_PACKET_SIZE )
		return;

	if( m_dwBufferSize < DWORD(GetSize() + wAddSize) )
		ExpandIoBuffer(GetSize() + wAddSize);

	memcpy( m_ptrOffset, pMsg->m_pBuf+PACKET_HEADER_SIZE+wDeleteSize, wAddSize);
	m_pHeader->m_wSize += wAddSize;
	m_ptrOffset += wAddSize;
}

void CPacket::AddData(CPacket * pMsg)
{
	if(!pMsg)
		return;

	WORD wDataSize = pMsg->GetSize() - PACKET_HEADER_SIZE;
	if( wDataSize+GetSize() >= MAX_PACKET_SIZE )
		return;

	if( m_dwBufferSize < DWORD(wDataSize + GetSize())  )
		ExpandIoBuffer(wDataSize + GetSize());

	memcpy( m_ptrOffset, pMsg->m_pBuf+PACKET_HEADER_SIZE, wDataSize);
	m_pHeader->m_wSize += wDataSize;
	m_ptrOffset += wDataSize;
}

int CPacket::DetachBinary( LPVOID ptr)
{
	int nLength;

	Read( &nLength, sizeof(DWORD));
	if( nLength <= 0 || !CanRead(nLength) )
		return 0;

//	*ptr = new BYTE[nLength];
	Read( ptr, nLength);

	return nLength;
}

int CPacket::DetachBinary( LPVOID ptr, DWORD maxBytes)
{
	// Safer overload — refuses to read more than the caller's buffer
	// can hold. The unbounded DetachBinary(LPVOID) above will read up
	// to MAX_PACKET_SIZE bytes (~64 KB) into whatever buffer the
	// caller passed, which is a 64 KB heap/stack overflow primitive
	// from any peer that can send a packet with a chosen length
	// prefix. New code MUST use this form.
	if( ptr == NULL || maxBytes == 0 )
		return 0;

	int nLength;
	Read( &nLength, sizeof(DWORD));

	// Bounds-check the on-wire length against:
	//   * positive (Read already rejects non-positive but be explicit)
	//   * within MAX_PACKET_SIZE (CanRead enforces this too, kept for clarity)
	//   * within the caller-provided buffer size
	if( nLength <= 0 ||
	    static_cast<DWORD>(nLength) > maxBytes ||
	    !CanRead(static_cast<DWORD>(nLength)) )
		return 0;

	Read( ptr, nLength);
	return nLength;
}

void CPacket::AttachBinary( LPVOID param, int nLength)
{
	Write( &nLength, sizeof(int));
	Write( param, nLength);
}

DWORD CPacket::GetReadBytes()
{
	return m_dwReadBytes;
}

BOOL CPacket::ReadBytes( DWORD dwReadBytes)
{
	DWORD dwTotalBytes = m_dwReadBytes + dwReadBytes;

	if( dwTotalBytes > m_dwBufferSize)
		return FALSE;

	m_dwReadBytes += dwReadBytes;
	return TRUE;
}

CPacket& CPacket::operator << ( DWORD param)
{
	Write( (LPVOID) &param, sizeof(DWORD));
	return *this;
}

CPacket& CPacket::operator << ( WORD param)
{
	Write( (LPVOID) &param, sizeof(WORD));
	return *this;
}

CPacket& CPacket::operator << ( LPCTSTR param)
{
	int nLength = lstrlen(param);

	Write( (LPVOID) &nLength, sizeof(int));
	Write( (LPVOID) param, nLength);

	return *this;
}

CPacket& CPacket::operator << ( short param)
{
	Write( (LPVOID) &param, sizeof(short));
	return *this;
}

CPacket& CPacket::operator << ( long param)
{
	Write( (LPVOID) &param, sizeof(long));
	return *this;
}

CPacket& CPacket::operator << ( int param)
{
	Write( (LPVOID) &param, sizeof(int));
	return *this;
}

CPacket& CPacket::operator << ( char param)
{
	Write( (LPVOID) &param, sizeof(char));
	return *this;
}

CPacket& CPacket::operator << ( BYTE param)
{
	Write( (LPVOID) &param, sizeof(BYTE));
	return *this;
}

CPacket& CPacket::operator << ( float param)
{
	Write( (LPVOID)&param, sizeof(float));
	return *this;
}

CPacket& CPacket::operator << ( __int64 param)
{
	Write( (LPVOID) &param, sizeof(__int64));
	return *this;
}

CPacket& CPacket::operator >> ( DWORD& param)
{
	Read( (LPVOID) &param, sizeof(DWORD));
	return *this;
}

CPacket& CPacket::operator >> ( WORD& param)
{
	Read( (LPVOID) &param, sizeof(WORD));
	return *this;
}

CPacket& CPacket::operator >> ( CString& param)
{
	int nLength = 0;

	Read( (LPVOID) &nLength, sizeof(int));

	// Security fix: nLength is a signed int read from a wire packet.
	// Previously this was passed straight to CanRead(DWORD) and then
	// `new char[nLength + 1]` and `Read(buff, nLength)`. Three bugs
	// converged into a remote, pre-auth heap-corruption primitive
	// reachable from every CS_ handler that reads a CString
	// (e.g. CS_LOGIN_REQ → m_strUserID, m_strPasswd):
	//   1. CanRead's DWORD math wrapped around for negative input
	//      and returned TRUE (now also hardened in CanRead itself).
	//   2. `new char[nLength + 1]` with negative nLength either
	//      allocated 0 bytes or threw bad_alloc.
	//   3. Read(buff, nLength) → memcpy(buff, src, (size_t)nLength)
	//      with size_t(-1) = ~16 EB → heap obliteration.
	// Reject anything that isn't a sane in-bounds length. Caller
	// gets an empty CString, matching the legacy "read past end of
	// packet" silent-empty behavior — see COMPLETENESS_ANALYSIS.md
	// §3.2 documenting that behavior on the C# port side.
	if (nLength < 0 || nLength > static_cast<int>(MAX_PACKET_SIZE))
		return *this;

	if(CanRead(static_cast<DWORD>(nLength)))
	{
		LPTSTR buff = new char[nLength + 1];

		memset( buff, '\0', nLength + 1);
		Read( buff, nLength);
		param = buff;

		delete[] buff;
	}

	return *this;
}

CPacket& CPacket::operator >> ( short& param)
{
	Read( (LPVOID) &param, sizeof(short));
	return *this;
}

CPacket& CPacket::operator >> ( long& param)
{
	Read( (LPVOID) &param, sizeof(long));
	return *this;
}

CPacket& CPacket::operator >> ( int& param)
{
	Read( (LPVOID) &param, sizeof(int));
	return *this;
}

CPacket& CPacket::operator >> ( char& param)
{
	Read( (LPVOID) &param, sizeof(char));
	return *this;
}

CPacket& CPacket::operator >> ( BYTE& param)
{
	Read( (LPVOID) &param, sizeof(BYTE));
	return *this;
}

CPacket& CPacket::operator >> ( float& param)
{
	Read( (LPVOID) &param, sizeof(float));
	return *this;
}

CPacket& CPacket::operator >> ( __int64& param)
{
	Read( (LPVOID) &param, sizeof(__int64));
	return *this;
}

void CPacket::Rewind(BOOL bWrite)
{
	if(bWrite)
		m_pHeader->m_wSize = PACKET_HEADER_SIZE;
	m_ptrOffset = m_pBuf + PACKET_HEADER_SIZE;
}
