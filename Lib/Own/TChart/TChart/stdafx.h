#pragma once

#define WIN32_LEAN_AND_MEAN
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
#define VC_EXTRALEAN
#define NO_WARN_MBCS_MFC_DEPRECATION
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#pragma warning( disable : 4312 4091)

#include <afx.h>
#include <afxwin.h>
#include <afxsock.h>

// STL
#include <vector>
#include <map>
#include <set>

using namespace std;

#include <NetCode.h>
#include <d3dx9.h>

#include "TChartType.h"