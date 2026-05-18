# Root CMake for 4Story server modernization.
#
# Status: Phase 1 (TNetLib modernization). The legacy MSBuild solution at
# Server/TServer.sln remains authoritative for Windows builds today; this
# CMake tree is being grown alongside it to enable Linux builds and a path
# to dropping ATL/MFC/Win32-specific surface.
#
# See _rewrite/docs/MODERNIZATION_PLAN.md for the full plan.

cmake_minimum_required(VERSION 3.20)

project(FourStory
    VERSION 5.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Where Windows builds land — outside the source tree.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Per-platform defaults. Servers historically depend on Win32 / ATL / MFC;
# the modernization migrates each piece individually. Until a component is
# ported, FOURSTORY_HAS_WIN32 gates the legacy code paths.
if(WIN32)
    set(FOURSTORY_HAS_WIN32 ON)
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0601
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
        NO_WARN_MBCS_MFC_DEPRECATION
        _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
    )
    add_compile_options(/W3 /MP /wd4091)
else()
    set(FOURSTORY_HAS_WIN32 OFF)
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# Common include roots, used by every TNetLib consumer.
set(FOURSTORY_TPROTOCOL_INCLUDE_DIR
    ${CMAKE_SOURCE_DIR}/Lib/Own/TProtocol/include
    CACHE PATH "TProtocol wire definitions header dir")

# Vendored OpenSSL lives at Lib/3rdParty/openssl. On Windows we link the
# vendored copy; on Linux we expect a system-installed OpenSSL (libssl-dev).
# Migrating the Win32 CryptoAPI calls in TNetLib/CryptographyExt.cpp to
# OpenSSL EVP is a Phase 1 deliverable.
find_package(OpenSSL QUIET)
if(NOT OpenSSL_FOUND AND WIN32)
    set(_VENDORED_OPENSSL "${CMAKE_SOURCE_DIR}/Lib/3rdParty/openssl")
    if(EXISTS "${_VENDORED_OPENSSL}/include")
        message(STATUS "Using vendored OpenSSL at ${_VENDORED_OPENSSL}")
        set(OPENSSL_INCLUDE_DIR "${_VENDORED_OPENSSL}/include")
        # Library paths get wired up in the consuming target once we know the
        # build variant (Win32 .lib layout in Lib/3rdParty/openssl/lib/).
        set(OpenSSL_FOUND ON)
    endif()
endif()

# ODBC — kept as the SQL layer. Native MS ODBC on Windows, unixODBC on
# Linux. The CSqlBase/CSqlQuery surface in TNetLib stays the same; only
# the Win32-specific glue around it (string types, paths) gets modernized.
if(WIN32)
    set(FOURSTORY_ODBC_LIB odbc32)
else()
    find_package(ODBC QUIET)
    if(ODBC_FOUND)
        set(FOURSTORY_ODBC_LIB ODBC::ODBC)
    else()
        message(WARNING
            "unixODBC not found — TNetLib SQL layer will not link on this host. "
            "Install libodbc2-dev / unixodbc-dev to enable.")
    endif()
endif()

# Test infrastructure (must be enabled at the top level so add_test()
# calls inside subprojects register with ctest).
include(CTest)
enable_testing()

# Subprojects. Add new servers / libs here as they get a CMakeLists.txt.
add_subdirectory(Lib/Own/TNetLib)
