# TPatchSvrAsio — modernized 4Story patch metadata server. Plain-wire
# 8B-header codec (no XOR / no RC4 — legacy TPatchSvr uses raw packets),
# SOCI queries against TGLOBAL.TVERSION / TPREVERSION, 9 CT_* handlers.

set(TPATCHSVR_ASIO_SRC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(spdlog QUIET)
find_package(tomlplusplus QUIET)
find_package(SOCI CONFIG QUIET)

set(TPATCHSVR_SOCI_LIBS "")
if(SOCI_FOUND)
    if(TARGET SOCI::soci_core)
        list(APPEND TPATCHSVR_SOCI_LIBS SOCI::soci_core)
    elseif(TARGET SOCI::soci_core_static)
        list(APPEND TPATCHSVR_SOCI_LIBS SOCI::soci_core_static)
    endif()
    if(TARGET SOCI::soci_odbc)
        list(APPEND TPATCHSVR_SOCI_LIBS SOCI::soci_odbc)
    elseif(TARGET SOCI::soci_odbc_static)
        list(APPEND TPATCHSVR_SOCI_LIBS SOCI::soci_odbc_static)
    endif()
else()
    list(APPEND TPATCHSVR_SOCI_LIBS soci_odbc soci_core dl)
endif()

add_library(tpatchsvr_asio_core STATIC
    ${TPATCHSVR_ASIO_SRC}/patch_server.cpp
    ${TPATCHSVR_ASIO_SRC}/handlers.cpp
    ${TPATCHSVR_ASIO_SRC}/config.cpp
    ${TPATCHSVR_ASIO_SRC}/patch_session.cpp
    ${TPATCHSVR_ASIO_SRC}/services/patch_repository.cpp
    ${TPATCHSVR_ASIO_SRC}/db/schema_validator.cpp)

# Shared infrastructure (DB pool, audit, admin shell, health endpoint,
# rate limiter, registry refresher) lives in fourstory_common. tnetlib
# is the wire-codec layer; included for MessageId / AsioSession.
target_include_directories(tpatchsvr_asio_core PUBLIC
    ${TPATCHSVR_ASIO_SRC})

target_link_libraries(tpatchsvr_asio_core PUBLIC
    fourstory_common
    tnetlib_portable
    ${TPATCHSVR_SOCI_LIBS})

if(spdlog_FOUND)
    target_link_libraries(tpatchsvr_asio_core PUBLIC spdlog::spdlog)
else()
    target_link_libraries(tpatchsvr_asio_core PUBLIC spdlog)
endif()
if(tomlplusplus_FOUND)
    target_link_libraries(tpatchsvr_asio_core PUBLIC tomlplusplus::tomlplusplus)
endif()
set_target_properties(tpatchsvr_asio_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(tpatchsvr_asio ${TPATCHSVR_ASIO_SRC}/main.cpp)
target_link_libraries(tpatchsvr_asio PRIVATE tpatchsvr_asio_core)

# ----- Tests --------------------------------------------------------
#
# Pure-unit: stale-client sweep semantics — opens loopback sockets to
# manufacture peered PatchSessions, exercises the sweep against the
# real predicate (IsOpen + IsServerPeer + ConnectedAt). Runs anywhere
# we can build the binary.
add_executable(test_tpatchsvr_asio_stale_sweep
    ${TPATCHSVR_ASIO_SRC}/tests/test_stale_sweep.cpp)
target_link_libraries(test_tpatchsvr_asio_stale_sweep PRIVATE
    tpatchsvr_asio_core)
add_test(NAME tpatchsvr_asio_stale_sweep
    COMMAND test_tpatchsvr_asio_stale_sweep)

# Integration: PatchRepository + schema validator against a live DB.
# Skips when neither TPATCHSVR_TEST_PG_CONN nor TPATCHSVR_TEST_MSSQL_CONN
# is in the environment. Covers all four round-1 audit fixes
# (P-1..P-4 regressions) plus the round-2 promote-pre-version
# (P-5) and the F5 schema validator.
add_executable(test_tpatchsvr_asio_soci_repository
    ${TPATCHSVR_ASIO_SRC}/tests/test_soci_patch_repository.cpp)
target_link_libraries(test_tpatchsvr_asio_soci_repository PRIVATE
    tpatchsvr_asio_core)
add_test(NAME tpatchsvr_asio_soci_repository
    COMMAND test_tpatchsvr_asio_soci_repository)
