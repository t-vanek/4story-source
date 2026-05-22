# Root CMake for 4Story server modernization.
#
# Status: Phase 1 (TNetLib modernization). The legacy MSBuild solution at
# Server/TServer.sln remains authoritative for Windows builds today; this
# CMake tree is being grown alongside it to enable Linux builds and a path
# to dropping ATL/MFC/Win32-specific surface.
#
# See _rewrite/docs/MODERNIZATION_PLAN.md for the full plan.

cmake_minimum_required(VERSION 3.20)

# CMP0167: prefer Boost's upstream BoostConfig package (BoostConfig.cmake
# shipped by vcpkg) over the CMake-bundled FindBoost module, which CMake
# 4.x removed. Setting OLD here keeps the existing find_package(Boost)
# call working through the legacy module — both modes resolve the same
# vcpkg-installed Boost.
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

project(FourStory
    VERSION 5.0
    LANGUAGES C CXX
)

# Fallback CMAKE_PREFIX_PATH when the vcpkg toolchain integration
# isn't active (e.g. someone reconfigured without
# `-DCMAKE_TOOLCHAIN_FILE=…/vcpkg.cmake`, or the toolchain key got
# cleared from the cache). The manifest-mode install dir is stable
# at build/vcpkg_installed/<triplet>/; pointing CMAKE_PREFIX_PATH
# there lets `find_package(Boost CONFIG)`, `find_package(spdlog)`,
# etc. resolve without re-running vcpkg install.
if(WIN32)
    set(_fourstory_vcpkg_prefix
        "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
    if(EXISTS "${_fourstory_vcpkg_prefix}/share")
        list(APPEND CMAKE_PREFIX_PATH "${_fourstory_vcpkg_prefix}")
    endif()
    unset(_fourstory_vcpkg_prefix)
endif()

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

# Locate OpenSSL 3's `legacy` provider DLL/.so so we can deploy it
# alongside binaries that exercise RC4 (the modernized wire codec).
# vcpkg's auto-deploy only follows link-time DLL dependencies; the
# legacy provider is loaded at runtime via OSSL_PROVIDER_load, so
# without an explicit copy step it lands in vcpkg_installed/.../bin
# but never in build/bin where the test executables live.
#
# The fourstory_deploy_ossl_legacy(<target>) function below adds a
# POST_BUILD step that copies legacy.dll (Windows) or legacy.so
# (Linux) into the target's runtime directory if found. No-op when
# the provider isn't shipped.
set(FOURSTORY_OSSL_LEGACY_MODULE "" CACHE FILEPATH
    "Path to OpenSSL 3 legacy provider module (legacy.dll / legacy.so)")
if(NOT FOURSTORY_OSSL_LEGACY_MODULE)
    if(WIN32)
        # vcpkg layout: lives in bin/ next to libcrypto-3-x64.dll. We
        # don't rely on OpenSSL_FOUND or VCPKG_INSTALLED_DIR being set
        # at root scope (vcpkg manifest integration may resolve them
        # later inside add_subdirectory) — probe the well-known build-
        # tree layout directly. The path is deterministic for vcpkg
        # manifest mode (build/vcpkg_installed/<triplet>/...).
        set(_fourstory_legacy_candidates
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin/legacy.dll"
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/bin/legacy.dll"
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x86-windows/bin/legacy.dll"
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x86-windows/debug/bin/legacy.dll")
        foreach(_cand IN LISTS _fourstory_legacy_candidates)
            if(EXISTS "${_cand}")
                set(FOURSTORY_OSSL_LEGACY_MODULE "${_cand}" CACHE FILEPATH
                    "Path to OpenSSL 3 legacy provider module" FORCE)
                break()
            endif()
        endforeach()
        unset(_fourstory_legacy_candidates)
    else()
        find_file(FOURSTORY_OSSL_LEGACY_MODULE
            NAMES legacy.so
            HINTS "/usr/lib/x86_64-linux-gnu/ossl-modules"
                  "/usr/lib/ossl-modules"
                  "/usr/local/lib/ossl-modules"
            NO_DEFAULT_PATH)
    endif()
endif()
if(FOURSTORY_OSSL_LEGACY_MODULE)
    message(STATUS "OpenSSL legacy provider: ${FOURSTORY_OSSL_LEGACY_MODULE}")
else()
    message(STATUS "OpenSSL legacy provider: not found "
        "— RC4 will fail at runtime in modern binaries.")
endif()

function(fourstory_deploy_ossl_legacy target)
    if(NOT FOURSTORY_OSSL_LEGACY_MODULE)
        return()
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${FOURSTORY_OSSL_LEGACY_MODULE}"
            "$<TARGET_FILE_DIR:${target}>"
        VERBATIM)
endfunction()

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
add_subdirectory(Lib/3rdParty/bcrypt)
add_subdirectory(Lib/Own/TNetLib)
add_subdirectory(Lib/Own/FourStoryCommon)
add_subdirectory(Server/TLoginSvrAsio)
add_subdirectory(Server/TPatchSvrAsio)
add_subdirectory(Server/TLogSvrAsio)
add_subdirectory(Server/TControlSvrAsio)
add_subdirectory(Server/TMapSvrAsio)
add_subdirectory(Server/TWorldSvrAsio)
