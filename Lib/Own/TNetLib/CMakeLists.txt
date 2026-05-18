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
    ${TNETLIB_SRC_DIR}/Session.cpp
    ${TNETLIB_SRC_DIR}/SqlBase.cpp
    ${TNETLIB_SRC_DIR}/SqlDatabase.cpp
    ${TNETLIB_SRC_DIR}/SqlDirect.cpp
    ${TNETLIB_SRC_DIR}/SqlQuery.cpp
    ${TNETLIB_SRC_DIR}/platform.cpp
    ${TNETLIB_SRC_DIR}/tnetlib_crypto.cpp
    ${TNETLIB_SRC_DIR}/asio_session.cpp
    ${TNETLIB_SRC_DIR}/packet_codec.cpp
)

set(TNETLIB_HEADERS
    ${TNETLIB_SRC_DIR}/BindDesc.h
    ${TNETLIB_SRC_DIR}/CryptographyExt.h
    ${TNETLIB_SRC_DIR}/ErrorCode.h
    ${TNETLIB_SRC_DIR}/Packet.h
    ${TNETLIB_SRC_DIR}/Session.h
    ${TNETLIB_SRC_DIR}/SqlBase.h
    ${TNETLIB_SRC_DIR}/SqlDatabase.h
    ${TNETLIB_SRC_DIR}/SqlDirect.h
    ${TNETLIB_SRC_DIR}/SqlQuery.h
    ${TNETLIB_SRC_DIR}/TNetDef.h
    ${TNETLIB_SRC_DIR}/TNetLib.h
    ${TNETLIB_SRC_DIR}/platform.h
    ${TNETLIB_SRC_DIR}/tnetlib_crypto.h
    ${TNETLIB_SRC_DIR}/asio_session.h
    ${TNETLIB_SRC_DIR}/packet_codec.h
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

    # platform.cpp, tnetlib_crypto.cpp, and asio_session.cpp are PCH-free
    # — they deliberately do NOT include stdafx.h (which drags in
    # winsock2.h). Opt them out of PCH on Windows so the two toolchains
    # compile them identically.
    set_source_files_properties(
        ${TNETLIB_SRC_DIR}/platform.cpp
        ${TNETLIB_SRC_DIR}/tnetlib_crypto.cpp
        ${TNETLIB_SRC_DIR}/asio_session.cpp
        ${TNETLIB_SRC_DIR}/packet_codec.cpp
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif()

# Boost.Asio link surface for asio_session.cpp. The header is mostly
# header-only but boost_system is needed for error_code types.
if(Boost_FOUND)
    target_link_libraries(tnetlib PUBLIC Boost::system)
else()
    # Fallback for hosts where find_package(Boost) failed but
    # libboost-system is available as a plain library (some Linux
    # distros without CMake config files).
    target_link_libraries(tnetlib PUBLIC boost_system)
endif()

# Standalone crypto test (no external test framework needed — pure
# C++ + OpenSSL). Run via `ctest --test-dir build -R tnetlib_crypto`.
if(NOT WIN32 OR FOURSTORY_FORCE_OPENSSL_CRYPTO)
    if(NOT OpenSSL_FOUND)
        find_package(OpenSSL REQUIRED)
    endif()
    add_executable(test_tnetlib_crypto
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_crypto.cpp
        ${TNETLIB_SRC_DIR}/tnetlib_crypto.cpp)
    target_include_directories(test_tnetlib_crypto PRIVATE ${TNETLIB_SRC_DIR})
    target_link_libraries(test_tnetlib_crypto PRIVATE OpenSSL::Crypto)
    set_source_files_properties(
        ${TNETLIB_SRC_DIR}/tnetlib_crypto.cpp
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON
        TARGET_DIRECTORY test_tnetlib_crypto)

    include(CTest)
    add_test(NAME tnetlib_crypto_kat COMMAND test_tnetlib_crypto)
endif()

# Asio session test — built whenever Boost is available, regardless of
# the crypto backend choice. Standalone executable; exercises the
# accept + read + write happy path with a localhost echo round-trip.
if(Boost_FOUND OR EXISTS /usr/include/boost/asio.hpp)
    add_executable(test_tnetlib_asio
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_asio_session.cpp
        ${TNETLIB_SRC_DIR}/asio_session.cpp
        ${TNETLIB_SRC_DIR}/packet_codec.cpp)
    target_include_directories(test_tnetlib_asio PRIVATE ${TNETLIB_SRC_DIR})
    if(Boost_FOUND)
        target_link_libraries(test_tnetlib_asio PRIVATE Boost::system)
    else()
        target_link_libraries(test_tnetlib_asio PRIVATE boost_system)
    endif()
    if(NOT WIN32)
        # On POSIX, the Asio io_context background thread + sync client
        # socket needs pthread for the std::thread join.
        target_link_libraries(test_tnetlib_asio PRIVATE pthread)
    endif()
    set_source_files_properties(
        ${TNETLIB_SRC_DIR}/asio_session.cpp
        ${TNETLIB_SRC_DIR}/packet_codec.cpp
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON
        TARGET_DIRECTORY test_tnetlib_asio)

    add_test(NAME tnetlib_asio_echo COMMAND test_tnetlib_asio)
endif()

# Packet codec test — pure C++20, no Boost or OpenSSL needed.
add_executable(test_tnetlib_packet_codec
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_packet_codec.cpp
    ${TNETLIB_SRC_DIR}/packet_codec.cpp)
target_include_directories(test_tnetlib_packet_codec PRIVATE ${TNETLIB_SRC_DIR})
set_source_files_properties(
    ${TNETLIB_SRC_DIR}/packet_codec.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
    TARGET_DIRECTORY test_tnetlib_packet_codec)
add_test(NAME tnetlib_packet_codec COMMAND test_tnetlib_packet_codec)

# Crypto backend selection:
#   - Non-Windows: must use OpenSSL (no Win32 CryptoAPI exists)
#   - Windows:     defaults to Win32 CryptoAPI for ABI-stable backward
#                  compatibility. Set FOURSTORY_FORCE_OPENSSL_CRYPTO=ON
#                  to opt into the OpenSSL path on Windows (e.g. for
#                  testing equivalence before the eventual cutover).
option(FOURSTORY_FORCE_OPENSSL_CRYPTO
    "Use OpenSSL EVP for TNetLib crypto even on Windows" OFF)
if(NOT WIN32 OR FOURSTORY_FORCE_OPENSSL_CRYPTO)
    target_compile_definitions(tnetlib PRIVATE FOURSTORY_USE_OPENSSL_CRYPTO=1)
    if(NOT OpenSSL_FOUND)
        find_package(OpenSSL REQUIRED)
    endif()
    target_link_libraries(tnetlib PUBLIC OpenSSL::Crypto)
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
