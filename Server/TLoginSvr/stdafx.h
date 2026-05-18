#pragma once	

// TODO: Enable precompiled Headers again

#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
#define _ATL_NO_COM_SUPPORT
#define _ATL_ALL_WARNINGS

// Global include
#include "..\global\globalinc.h"

//
//	Log Server	UDP Send Define
//
// #define	DEF_UDPLOG

#include "resource.h"
#include <TNetLib.h>

#include <NetCode.h>
#include "TLoginType.h"
#include <SSProtocol.h>
#include <CSProtocol.h>
#include <CTProtocol.h>
#include "DBAccess.h"
#include "DebugSocket.h"


#ifdef DEF_UDPLOG
	#include "UdpSocket.h"
#endif
