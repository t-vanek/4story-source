#if !defined __TNETDEF_H
#define __TNETDEF_H


//////////////////////////////////////////////////////////////////////////////
// TNetLib constant define

#define ONE_KBYTE					(1024)
#define MAX_THREAD					(256)


//////////////////////////////////////////////////////////////////////////////
// Default port

#define DEF_CONTROLPORT				(3615)
#define DEF_PATCHPORT				(3715)
#define DEF_WORLDPORT				(3815)
#define DEF_LOGINPORT				(4815)
#define DEF_MAPPORT					(5815)

//////////////////////////////////////////////////////////////////////////////
// Completion key
#define COMP_NULL					((BYTE) 0)
#define COMP_ACCEPT					((BYTE) 1)
#define COMP_SESSION				((BYTE) 2)
#define COMP_CLOSE					((BYTE) 3)
#define COMP_SEND					((BYTE) 4)
#define COMP_EXIT					((BYTE) 255)

//////////////////////////////////////////////////////////////////////////////
// TNetLib macro

#define SMART_LOCKCS(x)				CCSLock lock_object(x);

// Portable replacements for the previous ATL CTime-based conversions.
// CTime is MFC/ATL — Windows-only and adds an ATL link dependency for
// callers that just want time_t↔SQL_TIMESTAMP. Using <time.h> directly
// keeps the helpers usable from Linux builds.
inline __time64_t __DBTOTIME(TIMESTAMP_STRUCT timestamp)
{
	if(timestamp.year < 2000)
		return 0;

	struct tm t = {};
	t.tm_year = timestamp.year - 1900;
	t.tm_mon  = timestamp.month - 1;
	t.tm_mday = timestamp.day;
	t.tm_hour = timestamp.hour;
	t.tm_min  = timestamp.minute;
	t.tm_sec  = timestamp.second;
	t.tm_isdst = -1; // let the runtime decide DST — matches CTime ctor semantics
	return (__time64_t)mktime(&t);
};

inline void __TIMETODB(__time64_t time_t, TIMESTAMP_STRUCT & timestamp)
{
	if(!time_t)
	{
		timestamp.year = 1900;
		timestamp.month = 1;
		timestamp.day = 1;
		timestamp.hour = 0;
		timestamp.minute = 0;
		timestamp.second = 0;
		timestamp.fraction = 0;
	}
	else
	{
		::time_t raw = (::time_t)time_t;
		struct tm t = {};
#if defined(_WIN32)
		localtime_s(&t, &raw);
#else
		localtime_r(&raw, &t);
#endif
		timestamp.year   = (SQLSMALLINT)(t.tm_year + 1900);
		timestamp.month  = (SQLUSMALLINT)(t.tm_mon + 1);
		timestamp.day    = (SQLUSMALLINT)t.tm_mday;
		timestamp.hour   = (SQLUSMALLINT)t.tm_hour;
		timestamp.minute = (SQLUSMALLINT)t.tm_min;
		timestamp.second = (SQLUSMALLINT)t.tm_sec;
		timestamp.fraction = 0;
	}
};

inline DWORD TRand(DWORD dwMaxNumber)
{
	if(dwMaxNumber <= 1)
		return 0;

	DWORD dwRand = 0;
	WORD wCM = 0;

	if(dwMaxNumber > 0xFFFFFF)
	{
		dwRand = (rand() % HIBYTE(HIWORD(dwMaxNumber))) << 24;
		wCM = WORD((dwMaxNumber - (dwMaxNumber & 0xFF000000)) >> 16) + 0x100;
	}

	if(dwMaxNumber > 0xFFFF)
	{
		dwRand = dwRand + (rand() % max(wCM, LOBYTE(HIWORD(dwMaxNumber))) << 16);
		wCM = WORD((dwMaxNumber - (dwMaxNumber & 0xFFFF0000)) >> 8) + 0x100;
	}

	if(dwMaxNumber > 0xFF)
	{
		dwRand = dwRand + (rand() % max(wCM, HIBYTE(LOWORD(dwMaxNumber))) << 8);
		wCM = WORD(dwMaxNumber - (dwMaxNumber & 0xFFFFFF00) + 0x100);
	}

	dwRand = dwRand + (rand() % max(wCM, LOBYTE(LOWORD(dwMaxNumber))));

	return dwRand;
}

//////////////////////////////////////////////////////////////////////////////
// TNetLib type define

typedef struct tagPACKETBUF			PACKETBUF, *LPPACKETBUF;
typedef queue<LPPACKETBUF>			QPACKETBUF, *LPQPACKETBUF;

//////////////////////////////////////////////////////////////////////////////
// TNetLib structure

struct tagPACKETBUF
{
	CSession *m_pSESSION;
	CPacket m_packet;
	tagPACKETBUF()
	{
		m_pSESSION = NULL;
	};
};

//////////////////////////////////////////////////////////////////////////////
// Smart sync class for critical section

class CCSLock
{
public:
	CCSLock( CRITICAL_SECTION *pCS)
	{
		EnterCriticalSection(pCS);
		m_pCS = pCS;
	};

	virtual ~CCSLock()
	{
		LeaveCriticalSection(m_pCS);
	};

protected:
	CRITICAL_SECTION *m_pCS;
};


#endif // !defined __TNETDEF_H
