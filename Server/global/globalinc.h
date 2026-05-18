#pragma once

/*
These are the global defines for every project.
*/
#pragma warning( disable : 4091 )
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