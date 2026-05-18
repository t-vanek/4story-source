// platform.cpp is intentionally PCH-free (does not include stdafx.h) so it
// compiles in a Linux toolchain that has no precompiled header for
// TNetLib.h / winsock2.h. The MSVC vcxproj marks this file with
// PrecompiledHeader=NotUsing.
#include "platform.h"

#if defined(_WIN32)
    #define SECURITY_WIN32
    #include <Security.h>
    #pragma comment(lib, "secur32")
#endif

namespace tnetlib_platform {

std::string GetHostName()
{
#if defined(_WIN32)
    char buf[256] = { 0 };
    DWORD len = sizeof(buf);
    if (GetComputerNameExA(ComputerNameDnsFullyQualified, buf, &len))
    {
        return std::string(buf, len);
    }
    len = sizeof(buf);
    if (::GetComputerNameA(buf, &len))
    {
        return std::string(buf, len);
    }
    return "unknown";
#else
    char buf[256] = { 0 };
    if (::gethostname(buf, sizeof(buf) - 1) == 0)
    {
        return std::string(buf);
    }
    return "unknown";
#endif
}

} // namespace tnetlib_platform
