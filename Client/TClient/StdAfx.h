#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#endif

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit

// turns off MFC's hiding of some common and often safely ignored warning messages
#define _AFX_ALL_WARNINGS

#define _WIN32_DCOM 

// Global Include
#include "..\global\globalinc.h"

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdisp.h>        // MFC Automation classes

#include <afxdtctl.h>		// MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#include <afxsock.h>
#include <direct.h>
#include <dbghelp.h>

#include <assert.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <functional>
#include <tlhelp32.h>
#include <dxdiag.h>
#include <strsafe.h>
#include <winsvc.h>

//#define USE_HWID_AUTH // Toggle HWID Auth support, comment-out to disable. 

#ifdef TEST_MODE
#include <conio.h>
#endif

#ifdef USE_XTRAP
#include <XTrap4Client.h>
#endif

#ifdef USE_GG
#include "NPGameLib.h"
#endif

#include <T3D.h>
#include <CSProtocol.h>
#include <NetCode.h>
#include <TClientID.h>

#include <TChartType.h>
#include "TClientType.h"

#include "constant.hpp"
#include "helper.hpp"

#pragma comment(lib, "Rpcrt4.lib")
#endif // _AFX_NO_AFXCMN_SUPPORT
