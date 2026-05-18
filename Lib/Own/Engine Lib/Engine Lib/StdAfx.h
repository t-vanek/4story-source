#pragma once

#pragma warning( disable : 4786 4530 4503 4552)

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define NO_WARN_MBCS_MFC_DEPRECATION

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#ifdef _DEBUG
#ifdef USE_VLD
#include <vld.h>
#endif
#endif

#include <afx.h>
#include <afxwin.h>
#include <afxsock.h>
#include "T3D.h"
#include <zlib.h>
#include <shlwapi.h>
#include <Psapi.h>
#include "CryptographyExt.h"