#if !defined __TNETLIB_H
#define __TNETLIB_H

#include <winsock2.h>
#include <mswsock.h>
#include <atlbase.h>
#include <atlstr.h>
#include <atltime.h>

// STL header
#include <string>
#include <vector>
#include <queue>
#include <map>

// Tech debt: the legacy header had `using namespace ATL; using namespace std;`
// at this spot, which leaked the entirety of both namespaces into every
// consumer of TNetLib.h (~60 server source files). The narrowed list below
// preserves the names that consumers actually use unqualified — anything not
// listed here must be `std::`/`ATL::`-qualified at the call site. Phase 1
// cleanup will whittle this list down to zero as call sites are migrated.
using ATL::CString;
using ATL::CTime;
using std::string;
using std::vector;
using std::queue;
using std::map;
using std::make_pair;
using std::exception;
using std::runtime_error;
using std::ifstream;
using std::ofstream;
using std::fstream;

// DB Access header
#include <SQL.h>
#include <SQLExt.h>

#include <SqlDatabase.h>
#include <SqlBase.h>
#include <BindDesc.h>
#include <SqlQuery.h>
#include <SqlDirect.h>

// Network access header
#include <Packet.h>
#include <Session.h>
#include <TNetDef.h>
#include <ErrorCode.h>

#endif // !defined __TNETLIB_H
