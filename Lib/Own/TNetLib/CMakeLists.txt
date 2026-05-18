# TNetLib — shared networking infrastructure for every server.
#
# Source layout reminder:
#   Lib/Own/TNetLib/             ← this CMakeLists.txt
#       TNetLib/                 ← all sources + the public umbrella TNetLib.h
#       Lib/                     ← legacy MSBuild output dir (populated by VS)
#
# Phase 1 of the modernization plan targets this library. The legacy
# MSBuild project (TNetLib/TNetLib.vcxproj) remains the authoritative
# Windows build today; this CMakeLists.txt is the path off of it.

set(TNETLIB_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/TNetLib)

set(TNETLIB_SOURCES
    ${TNETLIB_SRC_DIR}/BindDesc.cpp
    ${TNETLIB_SRC_DIR}/CryptographyExt.cpp
    ${TNETLIB_SRC_DIR}/Packet.cpp
    ${TNETLIB_SRC_DIR}/Rijndael.cpp
    ${TNETLIB_SRC_DIR}/Session.cpp
    ${TNETLIB_SRC_DIR}/SqlBase.cpp
    ${TNETLIB_SRC_DIR}/SqlDatabase.cpp
    ${TNETLIB_SRC_DIR}/SqlDirect.cpp
    ${TNETLIB_SRC_DIR}/SqlQuery.cpp
    ${TNETLIB_SRC_DIR}/platform.cpp
)

set(TNETLIB_HEADERS
    ${TNETLIB_SRC_DIR}/BindDesc.h
    ${TNETLIB_SRC_DIR}/CryptographyExt.h
    ${TNETLIB_SRC_DIR}/ErrorCode.h
    ${TNETLIB_SRC_DIR}/Packet.h
    ${TNETLIB_SRC_DIR}/Rijndael.h
    ${TNETLIB_SRC_DIR}/Session.h
    ${TNETLIB_SRC_DIR}/SqlBase.h
    ${TNETLIB_SRC_DIR}/SqlDatabase.h
    ${TNETLIB_SRC_DIR}/SqlDirect.h
    ${TNETLIB_SRC_DIR}/SqlQuery.h
    ${TNETLIB_SRC_DIR}/TNetDef.h
    ${TNETLIB_SRC_DIR}/TNetLib.h
    ${TNETLIB_SRC_DIR}/platform.h
)

add_library(tnetlib STATIC ${TNETLIB_SOURCES} ${TNETLIB_HEADERS})
add_library(FourStory::TNetLib ALIAS tnetlib)

target_include_directories(tnetlib
    PUBLIC
        ${TNETLIB_SRC_DIR}
        ${FOURSTORY_TPROTOCOL_INCLUDE_DIR}
)

# Precompiled header — keeps the legacy stdafx.h pattern working under CMake
# on Windows. On Linux the PCH header drags in winsock2.h and would refuse to
# compile until the Boost.Asio migration lands, so we skip it entirely there
# and let each .cpp pull what it needs explicitly. Phase 1 work-in-progress.
if(WIN32)
    target_precompile_headers(tnetlib PRIVATE ${TNETLIB_SRC_DIR}/stdafx.h)

    # platform.cpp is the cross-platform shim — opts out of PCH even on
    # Windows so the two toolchains compile it identically (without
    # winsock2.h being pre-included).
    set_source_files_properties(${TNETLIB_SRC_DIR}/platform.cpp
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif()

# Platform-specific link surface.
#   Windows: ws2_32 + mswsock for IOCP/WSARecv, odbc32 for SQL,
#            advapi32 for Win32 CryptoAPI (CryptographyExt.cpp).
#   Linux:   pthread + dl + system OpenSSL + unixODBC.
# The big-ticket migration (IOCP → Boost.Asio, WinCrypt → OpenSSL EVP)
# happens inside the .cpp files; this section just lists the link surface.
if(WIN32)
    target_link_libraries(tnetlib
        PUBLIC
            ws2_32
            mswsock
            advapi32
            ${FOURSTORY_ODBC_LIB}
    )
else()
    target_link_libraries(tnetlib
        PUBLIC
            pthread
            dl
    )
    if(OpenSSL_FOUND)
        target_link_libraries(tnetlib PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    endif()
    if(FOURSTORY_ODBC_LIB)
        target_link_libraries(tnetlib PUBLIC ${FOURSTORY_ODBC_LIB})
    endif()
endif()

# Phase-1 work items, tracked via #ifdef so the legacy and modern code
# paths can coexist in tree during migration.
target_compile_definitions(tnetlib
    PRIVATE
        $<$<BOOL:${FOURSTORY_HAS_WIN32}>:FOURSTORY_TNETLIB_HAS_WIN32=1>
        $<$<NOT:$<BOOL:${FOURSTORY_HAS_WIN32}>>:FOURSTORY_TNETLIB_PORTABLE=1>
)
