#pragma once

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

typedef struct DXGI_JPEG_AC_HUFFMAN_TABLE {
	int CodeCounts[16];
	int CodeValues[162];
};

typedef struct DXGI_JPEG_DC_HUFFMAN_TABLE {
	int CodeCounts[12];
	int CodeValues[12];
};

typedef struct DXGI_JPEG_QUANTIZATION_TABLE {
	int Elements[64];
};

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxole.h>         // MFC OLE classes
#include <afxodlgs.h>       // MFC OLE dialog classes
#include <afxdisp.h>        // MFC Automation classes
#endif // _AFX_NO_OLE_SUPPORT


#ifndef _AFX_NO_DB_SUPPORT
#include <afxdb.h>			// MFC ODBC database classes
#endif // _AFX_NO_DB_SUPPORT

#ifndef _AFX_NO_DAO_SUPPORT
#include <afxdao.h>			// MFC DAO database classes
#endif // _AFX_NO_DAO_SUPPORT

#include <afxdtctl.h>		// MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#include <afxpriv.h>
#include <../src/mfc/afximpl.h>
#endif // _AFX_NO_AFXCMN_SUPPORT

#include <TUI.h>
