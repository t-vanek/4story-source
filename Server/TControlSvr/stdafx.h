#pragma once				

#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
#define _ATL_NO_COM_SUPPORT
#define _ATL_ALL_WARNINGS
#define _USE_MATH_DEFINES

// Global include
#include "..\global\globalinc.h"

#include <atltime.h>
#include "resource.h"
#include <TNetLib.h>

#include "TControlType.h"
#include "SSProtocol.h"
#include "CTProtocol.h"
#include "MWProtocol.h"
#include "NetCode.h"
#include "DBAccess.h"
#include "DebugSocket.h"
#include "PlatformUsage.h"