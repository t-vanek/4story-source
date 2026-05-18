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

#include <mutex>
#include <random>

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

std::uint64_t SecureRandom64()
{
    // Seeded once per process from std::random_device — which delegates
    // to /dev/urandom on POSIX and RtlGenRandom (BCryptGenRandom on
    // newer SDKs) on Windows. Both are CSPRNGs. The Mersenne twister
    // is then deterministic but the seed is unpredictable, which is
    // sufficient for session-key / nonce purposes; for actual key
    // material the caller should reach for OpenSSL's RAND_bytes via
    // tnetlib_crypto instead.
    //
    // Thread-safety: the static engine is guarded by a mutex. Cheap
    // enough at the rate we call this (login flows, session creation).
    static std::mt19937_64 engine{ std::random_device{}() };
    static std::mutex      engine_mtx;

    std::lock_guard<std::mutex> lock(engine_mtx);
    return engine();
}

} // namespace tnetlib_platform
