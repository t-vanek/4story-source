#pragma once

#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
#define _ATL_NO_COM_SUPPORT
#define _ATL_ALL_WARNINGS
#define _USE_MATH_DEFINES

// Global include
#include "..\global\globalinc.h"

//
//	Log Server	UDP Send Define
//
//#define	DEF_UDPLOG
//#define	DEF_UDPLOGGUILD

#define SKYGARDEN

#include <resource.h>

#include <TNetLib.h>
#include <NetCode.h>
#include "TMapType.h"
#include <SSProtocol.h>
#include <CSProtocol.h>
#include <CTProtocol.h>
#include <DMProtocol.h>
#include <MWProtocol.h>
#include "DBAccess.h"
#include "DebugSocket.h"

#ifdef DEF_UDPLOG
	#include "UdpSocket.h"
#endif

#define valdefinition "1: %s - 2: %s - 3: %s"