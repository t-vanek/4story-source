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

# OpenSSL — managed by vcpkg (see /vcpkg.json). The legacy vendored copy
# at Lib/3rdParty/openssl (OpenSSL 0.9.8l, Nov 2009, EOL 2015) was deleted
# during Phase 1 modernization because it was a dead reference (only
# CSmtp.cpp included it and CSmtp.cpp is not in any vcxproj).
#
# To configure: pass -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
# (or set VCPKG_ROOT and use CMake 3.21+ auto-detection).
# On Linux, a system-installed libssl-dev / openssl-devel also satisfies
# find_package(OpenSSL).
find_package(OpenSSL QUIET)

# Boost.Asio — used by the Phase-1 AsioSession scaffolding that will
# eventually replace the Win32 IOCP loops in CSession. Required for
# building the asio_session.cpp translation unit. Provided via vcpkg
# (boost-asio port) on Windows; system-installed libboost-system-dev
# on Linux works too. Asio itself is mostly header-only but
# boost_system is needed for error codes.
find_package(Boost QUIET COMPONENTS system)

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
add_subdirectory(Server/TLoginSvrAsio)
