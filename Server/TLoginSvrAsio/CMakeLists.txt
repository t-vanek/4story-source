# TLoginSvrAsio — modernized login server, runs alongside the legacy
# TLoginSvr.exe. Phase 3 of step E in the modernization plan; see
# README.md in this directory.

set(TLOGINSVR_ASIO_SRC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(spdlog QUIET)
find_package(tomlplusplus QUIET)

# libbcrypt — password verification for TACCOUNT_PW. Sources vendored at
# Lib/3rdParty/bcrypt (no working vcpkg port for libbcrypt; libxcrypt is
# Unix-only). Target is wired by the root CMakeLists.txt as
# fourstory_bcrypt.
set(TLOGINSVR_BCRYPT_LIB fourstory_bcrypt)

# SOCI — pluggable DB layer. Phase B. Distro packages on Debian/Ubuntu
# ship as plain libs (libsoci_core, libsoci_postgresql, …) without a
# CMake config file, so probe both paths: native find_package first
# (vcpkg-installed), then fall back to plain link names.
find_package(SOCI CONFIG QUIET)
set(TLOGINSVR_SOCI_LIBS "")
if(SOCI_FOUND)
    # vcpkg's SOCI exports targets as SOCI::soci_core / SOCI::soci_odbc
    # (and `_static` variants). Pick whichever flavor is present.
    if(TARGET SOCI::soci_core)
        list(APPEND TLOGINSVR_SOCI_LIBS SOCI::soci_core)
    elseif(TARGET SOCI::soci_core_static)
        list(APPEND TLOGINSVR_SOCI_LIBS SOCI::soci_core_static)
    endif()
    if(TARGET SOCI::soci_odbc)
        list(APPEND TLOGINSVR_SOCI_LIBS SOCI::soci_odbc)
    elseif(TARGET SOCI::soci_odbc_static)
        list(APPEND TLOGINSVR_SOCI_LIBS SOCI::soci_odbc_static)
    endif()
    if(TARGET SOCI::soci_sqlite3)
        list(APPEND TLOGINSVR_SOCI_LIBS SOCI::soci_sqlite3)
    elseif(TARGET SOCI::soci_sqlite3_static)
        list(APPEND TLOGINSVR_SOCI_LIBS SOCI::soci_sqlite3_static)
    endif()
    if(TARGET SOCI::soci_postgresql)
        list(APPEND TLOGINSVR_SOCI_LIBS SOCI::soci_postgresql)
    endif()
else()
    # Debian/Ubuntu: /usr/lib/x86_64-linux-gnu/libsoci_core.so and friends.
    # The order matters for static linkers; backends register themselves
    # at load time via soci::backend_factory globals. Postgres backend is
    # not in the vcpkg feature set right now (see vcpkg.json) — re-add
    # `soci_postgresql` here when re-enabled.
    list(APPEND TLOGINSVR_SOCI_LIBS
        soci_sqlite3
        soci_odbc
        soci_core
        dl)
endif()

# Shared library of the server's logic, so the integration test can
# instantiate LoginServer in-process instead of forking the binary.
add_library(tloginsvr_asio_core STATIC
    ${TLOGINSVR_ASIO_SRC}/login_server.cpp
    ${TLOGINSVR_ASIO_SRC}/admin_shell.cpp
    ${TLOGINSVR_ASIO_SRC}/handlers.cpp
    ${TLOGINSVR_ASIO_SRC}/config.cpp
    ${TLOGINSVR_ASIO_SRC}/health_endpoint.cpp
    ${TLOGINSVR_ASIO_SRC}/db/schema_validator.cpp
    ${TLOGINSVR_ASIO_SRC}/db/session_pool.cpp
    ${TLOGINSVR_ASIO_SRC}/services/fake_auth_service.cpp
    ${TLOGINSVR_ASIO_SRC}/services/local_connection_registry.cpp
    ${TLOGINSVR_ASIO_SRC}/services/local_event_registry.cpp
    ${TLOGINSVR_ASIO_SRC}/services/fake_map_server_locator.cpp
    ${TLOGINSVR_ASIO_SRC}/services/fake_session_terminator.cpp
    ${TLOGINSVR_ASIO_SRC}/services/fake_char_service.cpp
    ${TLOGINSVR_ASIO_SRC}/services/soci_auth_service.cpp
    ${TLOGINSVR_ASIO_SRC}/services/soci_char_service.cpp
    ${TLOGINSVR_ASIO_SRC}/services/soci_map_server_locator.cpp
    ${TLOGINSVR_ASIO_SRC}/services/soci_session_terminator.cpp
    ${TLOGINSVR_ASIO_SRC}/services/rate_limiter.cpp
    ${TLOGINSVR_ASIO_SRC}/services/registry_refresher.cpp
    ${TLOGINSVR_ASIO_SRC}/services/spdlog_audit_logger.cpp
    ${TLOGINSVR_ASIO_SRC}/services/spdlog_smtp_client.cpp
    ${TLOGINSVR_ASIO_SRC}/services/udp_audit_logger.cpp)
target_include_directories(tloginsvr_asio_core PUBLIC
    ${TLOGINSVR_ASIO_SRC}
    ${CMAKE_SOURCE_DIR}/Lib/Own/TNetLib/TNetLib
    ${FOURSTORY_TPROTOCOL_INCLUDE_DIR})

# SOCI's postgresql-backend header transitively #includes <libpq-fe.h>,
# which Debian ships under /usr/include/postgresql/ rather than the
# default include root. Use pkg-config when available so vcpkg or
# upstream-installed libpq is preferred over the distro layout.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBPQ QUIET libpq)
    if(LIBPQ_FOUND)
        target_include_directories(tloginsvr_asio_core PRIVATE ${LIBPQ_INCLUDE_DIRS})
    endif()
endif()
if(NOT LIBPQ_FOUND AND EXISTS /usr/include/postgresql/libpq-fe.h)
    target_include_directories(tloginsvr_asio_core PRIVATE /usr/include/postgresql)
endif()
target_link_libraries(tloginsvr_asio_core PUBLIC tnetlib_portable ${TLOGINSVR_SOCI_LIBS} ${TLOGINSVR_BCRYPT_LIB})
if(spdlog_FOUND)
    target_link_libraries(tloginsvr_asio_core PUBLIC spdlog::spdlog)
else()
    # Distro-installed without CMake config — fall back to the plain
    # library + header path. Works on Debian/Ubuntu where the package
    # is libspdlog-dev.
    target_link_libraries(tloginsvr_asio_core PUBLIC spdlog)
endif()
if(tomlplusplus_FOUND)
    target_link_libraries(tloginsvr_asio_core PUBLIC tomlplusplus::tomlplusplus)
endif()
# Else: toml++ is header-only on Debian/Ubuntu (libtomlplusplus-dev)
# so no link step is needed — the headers ship at /usr/include/toml++.
set_target_properties(tloginsvr_asio_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

# Entry-point binary.
add_executable(tloginsvr_asio ${TLOGINSVR_ASIO_SRC}/main.cpp)
target_link_libraries(tloginsvr_asio PRIVATE tloginsvr_asio_core)

# End-to-end handshake test.
add_executable(test_tloginsvr_asio_handshake
    ${TLOGINSVR_ASIO_SRC}/tests/test_handshake.cpp)
target_link_libraries(test_tloginsvr_asio_handshake PRIVATE
    tloginsvr_asio_core)
add_test(NAME tloginsvr_asio_handshake COMMAND test_tloginsvr_asio_handshake)

# Auth-flow test — exercises CS_LOGIN_REQ through the IAuthService
# interface with the in-memory backend.
add_executable(test_tloginsvr_asio_auth_flow
    ${TLOGINSVR_ASIO_SRC}/tests/test_auth_flow.cpp)
target_link_libraries(test_tloginsvr_asio_auth_flow PRIVATE
    tloginsvr_asio_core)
add_test(NAME tloginsvr_asio_auth_flow COMMAND test_tloginsvr_asio_auth_flow)

# Duplicate-kick test — two concurrent client AsioSessions log in as
# the same user; verifies the older session gets its socket closed.
add_executable(test_tloginsvr_asio_duplicate_kick
    ${TLOGINSVR_ASIO_SRC}/tests/test_duplicate_kick.cpp)
target_link_libraries(test_tloginsvr_asio_duplicate_kick PRIVATE
    tloginsvr_asio_core)
add_test(NAME tloginsvr_asio_duplicate_kick COMMAND test_tloginsvr_asio_duplicate_kick)

# Start-flow test — CS_START_REQ wired through IMapServerLocator,
# hit + miss scenarios.
add_executable(test_tloginsvr_asio_start_flow
    ${TLOGINSVR_ASIO_SRC}/tests/test_start_flow.cpp)
target_link_libraries(test_tloginsvr_asio_start_flow PRIVATE
    tloginsvr_asio_core)
add_test(NAME tloginsvr_asio_start_flow COMMAND test_tloginsvr_asio_start_flow)

# Session-terminator test — verifies the close-time cleanup chain
# (lookup → terminator → unregister) and the MapHandoff vs
# Disconnect reason flip.
add_executable(test_tloginsvr_asio_session_terminator
    ${TLOGINSVR_ASIO_SRC}/tests/test_session_terminator.cpp)
target_link_libraries(test_tloginsvr_asio_session_terminator PRIVATE
    tloginsvr_asio_core)
add_test(NAME tloginsvr_asio_session_terminator COMMAND test_tloginsvr_asio_session_terminator)

# Char-flow test — exercises CS_CHARLIST/CREATECHAR/DELCHAR through
# ICharService with the in-memory backend.
add_executable(test_tloginsvr_asio_char_flow
    ${TLOGINSVR_ASIO_SRC}/tests/test_char_flow.cpp)
target_link_libraries(test_tloginsvr_asio_char_flow PRIVATE
    tloginsvr_asio_core)
add_test(NAME tloginsvr_asio_char_flow COMMAND test_tloginsvr_asio_char_flow)

# SOCI integration tests — exercise the DB-backed service impls
# directly against a real PostgreSQL. All skipped automatically if
# TLOGINSVR_TEST_PG_CONN is not set so CI hosts without PG pass.
# Local run:
#   export TLOGINSVR_TEST_PG_CONN="host=localhost port=5432 \
#       dbname=tloginsvr_dev user=tloginsvr password=devpass"
#   ctest -R tloginsvr_asio_soci
add_executable(test_tloginsvr_asio_soci_auth
    ${TLOGINSVR_ASIO_SRC}/tests/test_soci_auth_flow.cpp)
target_link_libraries(test_tloginsvr_asio_soci_auth PRIVATE
    tloginsvr_asio_core)
add_test(NAME tloginsvr_asio_soci_auth COMMAND test_tloginsvr_asio_soci_auth)

add_executable(test_tloginsvr_asio_soci_char
    ${TLOGINSVR_ASIO_SRC}/tests/test_soci_char_flow.cpp)
target_link_libraries(test_tloginsvr_asio_soci_char PRIVATE
    tloginsvr_asio_core)
add_test(NAME tloginsvr_asio_soci_char COMMAND test_tloginsvr_asio_soci_char)

add_executable(test_tloginsvr_asio_soci_map
    ${TLOGINSVR_ASIO_SRC}/tests/test_soci_map_locator.cpp)
target_link_libraries(test_tloginsvr_asio_soci_map PRIVATE
    tloginsvr_asio_core)
add_test(NAME tloginsvr_asio_soci_map COMMAND test_tloginsvr_asio_soci_map)

add_executable(test_tloginsvr_asio_soci_session_terminator
    ${TLOGINSVR_ASIO_SRC}/tests/test_soci_session_terminator.cpp)
target_link_libraries(test_tloginsvr_asio_soci_session_terminator PRIVATE
    tloginsvr_asio_core)
add_test(NAME tloginsvr_asio_soci_session_terminator
    COMMAND test_tloginsvr_asio_soci_session_terminator)
