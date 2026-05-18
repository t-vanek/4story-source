#pragma once

#define WIN32_LEAN_AND_MEAN
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

// Carried forward from the now-deleted Server/TNetLib/stdafx.h (which included
// ../global/globalinc.h). Consolidated here so the single canonical TNetLib has
// the union of both copies' defines. Required to compile against the legacy
// MFC/MBCS surface used throughout the server tree.
#pragma warning( disable : 4091 )
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define NO_WARN_MBCS_MFC_DEPRECATION

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <TNetLib.h>
#include "CryptographyExt.h"