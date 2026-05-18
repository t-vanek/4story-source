#pragma once

// Cross-platform shim for the Win32 APIs that TNetLib historically reaches
// for directly. Bring this header in (or its eventual replacements) wherever
// you'd otherwise reach for <winsock2.h>, <windows.h>, etc.
//
// Scope of THIS file is intentionally narrow: only the type-level and
// header-include surface that lets non-async TNetLib code (Packet, SQL,
// helpers) be touched without dragging in winsock2.h. Async I/O
// (CreateIoCompletionPort, AcceptEx, OVERLAPPED, WSARecv) is NOT abstracted
// here — that piece moves to Boost.Asio in a later Phase 1 step, and shimming
// it byte-for-byte here would lock us into IOCP semantics that don't translate
// to epoll/io_uring.
//
// Migration pattern: when you touch a Win32-only API, first check whether
// this header already exposes a portable equivalent (e.g. tnetlib_GetHostName
// for GetComputerName). If yes, use it. If no and the surface is small
// enough, add it here. If the surface is large (any IOCP path), defer to the
// Boost.Asio milestone.

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <mswsock.h>
    #include <windows.h>
    // SOCKET, INVALID_SOCKET, SOCKET_ERROR, closesocket(), sockaddr_in
    // are all defined by the system headers above.
#else
    // POSIX equivalents for the socket-type surface that TNetLib touches.
    // Anything beyond this (IOCP, OVERLAPPED, AcceptEx, ConnectEx) intentionally
    // does NOT have a POSIX shim — the architectural move is to Boost.Asio.
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <cerrno>
    #include <cstdint>
    #include <cstring>

    using SOCKET = int;
    inline constexpr SOCKET INVALID_SOCKET = -1;
    inline constexpr int SOCKET_ERROR = -1;

    inline int closesocket(SOCKET s) { return ::close(s); }

    // ioctlsocket → ioctl with FIONBIO equivalent
    inline int ioctlsocket(SOCKET s, long cmd, unsigned long* argp)
    {
        if (cmd == /*FIONBIO*/ 0x8004667e)
        {
            int flags = ::fcntl(s, F_GETFL, 0);
            if (flags < 0) return SOCKET_ERROR;
            flags = *argp ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
            return ::fcntl(s, F_SETFL, flags) < 0 ? SOCKET_ERROR : 0;
        }
        errno = EINVAL;
        return SOCKET_ERROR;
    }
#endif

#include <string>

namespace tnetlib_platform {

// Replaces GetComputerNameEx(ComputerNameDnsFullyQualified, ...) with a
// portable equivalent. Returns hostname on POSIX, FQDN on Windows.
// Falls back to "unknown" if the OS can't tell us.
std::string GetHostName();

} // namespace tnetlib_platform
