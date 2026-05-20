# TControlSvrAsio — modernized 4Story control / orchestration daemon.
# F1 ships the scaffold: TOML config, accept loop, OperatorSession +
# fake auth/inventory services, post-login ack chain, health + admin.
# F2..F6 add peer dial, monitoring, admin operations, event scheduler,
# patch metadata, schema validator, and SOCI implementations.

set(TCONTROL_ASIO_SRC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(spdlog QUIET)
find_package(tomlplusplus QUIET)
find_package(SOCI CONFIG QUIET)

set(TCONTROL_SOCI_LIBS "")
if(SOCI_FOUND)
    if(TARGET SOCI::soci_core)
        list(APPEND TCONTROL_SOCI_LIBS SOCI::soci_core)
    elseif(TARGET SOCI::soci_core_static)
        list(APPEND TCONTROL_SOCI_LIBS SOCI::soci_core_static)
    endif()
    if(TARGET SOCI::soci_odbc)
        list(APPEND TCONTROL_SOCI_LIBS SOCI::soci_odbc)
    elseif(TARGET SOCI::soci_odbc_static)
        list(APPEND TCONTROL_SOCI_LIBS SOCI::soci_odbc_static)
    endif()
else()
    list(APPEND TCONTROL_SOCI_LIBS soci_odbc soci_core dl)
endif()

# Core library — shared between the binary and the test executable.
# Sources kept in one library so tests can link against the exact
# same TUs without rebuilding.
add_library(tcontrolsvr_asio_core STATIC
    ${TCONTROL_ASIO_SRC}/config.cpp
    ${TCONTROL_ASIO_SRC}/control_session.cpp
    ${TCONTROL_ASIO_SRC}/control_server.cpp
    ${TCONTROL_ASIO_SRC}/event_codec.cpp
    ${TCONTROL_ASIO_SRC}/event_scheduler.cpp
    ${TCONTROL_ASIO_SRC}/peer_dialer.cpp
    ${TCONTROL_ASIO_SRC}/senders.cpp
    ${TCONTROL_ASIO_SRC}/handlers/handlers_auth.cpp
    ${TCONTROL_ASIO_SRC}/handlers/handlers_service.cpp
    ${TCONTROL_ASIO_SRC}/handlers/handlers_admin.cpp
    ${TCONTROL_ASIO_SRC}/handlers/handlers_event.cpp
    ${TCONTROL_ASIO_SRC}/handlers/handlers_extra.cpp
    ${TCONTROL_ASIO_SRC}/handlers/handlers_patch.cpp
    ${TCONTROL_ASIO_SRC}/db/schema_validator.cpp
    ${TCONTROL_ASIO_SRC}/services/chat_ban_repository.cpp
    ${TCONTROL_ASIO_SRC}/services/event_registry.cpp
    ${TCONTROL_ASIO_SRC}/services/fake_operator_auth_service.cpp
    ${TCONTROL_ASIO_SRC}/services/operator_registry.cpp
    ${TCONTROL_ASIO_SRC}/services/peer_registry.cpp
    ${TCONTROL_ASIO_SRC}/services/soci_alerter.cpp
    ${TCONTROL_ASIO_SRC}/services/soci_event_repository.cpp
    ${TCONTROL_ASIO_SRC}/services/soci_operator_auth_service.cpp
    ${TCONTROL_ASIO_SRC}/services/soci_patch_metadata_service.cpp
    ${TCONTROL_ASIO_SRC}/services/soci_service_inventory.cpp
    ${TCONTROL_ASIO_SRC}/services/soci_user_protected_service.cpp
    ${TCONTROL_ASIO_SRC}/services/spdlog_admin_audit_logger.cpp
    ${TCONTROL_ASIO_SRC}/services/windows_scm_service_controller.cpp)

target_include_directories(tcontrolsvr_asio_core PUBLIC
    ${TCONTROL_ASIO_SRC})

target_link_libraries(tcontrolsvr_asio_core PUBLIC
    fourstory_common
    tnetlib_portable
    ${TCONTROL_SOCI_LIBS})

if(spdlog_FOUND)
    target_link_libraries(tcontrolsvr_asio_core PUBLIC spdlog::spdlog)
else()
    target_link_libraries(tcontrolsvr_asio_core PUBLIC spdlog)
endif()
if(tomlplusplus_FOUND)
    target_link_libraries(tcontrolsvr_asio_core PUBLIC tomlplusplus::tomlplusplus)
endif()
set_target_properties(tcontrolsvr_asio_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

add_executable(tcontrolsvr_asio ${TCONTROL_ASIO_SRC}/main.cpp)
target_link_libraries(tcontrolsvr_asio PRIVATE tcontrolsvr_asio_core)

# ----- Tests --------------------------------------------------------
#
# F1 wire test: stands the ControlServer up against an in-memory fake
# auth + inventory, connects a loopback client, and verifies the
# post-login ack chain (OPLOGIN, GROUPLIST, MACHINELIST, SVRTYPELIST,
# SERVICEAUTOSTART). Pure unit — no DB, no external services.
add_executable(test_tcontrolsvr_asio_operator_login
    ${TCONTROL_ASIO_SRC}/tests/test_operator_login.cpp)
target_link_libraries(test_tcontrolsvr_asio_operator_login PRIVATE
    tcontrolsvr_asio_core)
add_test(NAME tcontrolsvr_asio_operator_login
    COMMAND test_tcontrolsvr_asio_operator_login)

# F2 wire test: peer dial + SERVICEMONITOR fan-out. Stands up a loopback
# "peer" listener and verifies the full path CT_NEWCONNECT_REQ →
# dial → CT_CTRLSVR_REQ → CT_SERVICEMONITOR_REQ → CT_SERVICEMONITOR_ACK
# echo + CT_SERVICEDATA_ACK operator broadcast.
add_executable(test_tcontrolsvr_asio_peer_monitor
    ${TCONTROL_ASIO_SRC}/tests/test_peer_monitor.cpp)
target_link_libraries(test_tcontrolsvr_asio_peer_monitor PRIVATE
    tcontrolsvr_asio_core)
add_test(NAME tcontrolsvr_asio_peer_monitor
    COMMAND test_tcontrolsvr_asio_peer_monitor)

# F3 wire test: admin forwarders (kick, ban, chat-ban N-wave) +
# authority gate behavior. Pre-stages peer sockets in PeerRegistry
# without going through the dialer, drives operator wire packets,
# and asserts on FakeAdminAudit + FakeUserProtected records.
add_executable(test_tcontrolsvr_asio_admin_forwarders
    ${TCONTROL_ASIO_SRC}/tests/test_admin_forwarders.cpp)
target_link_libraries(test_tcontrolsvr_asio_admin_forwarders PRIVATE
    tcontrolsvr_asio_core)
add_test(NAME tcontrolsvr_asio_admin_forwarders
    COMMAND test_tcontrolsvr_asio_admin_forwarders)

# F4 unit test: StepScheduler state machine — pure logic, no asio.
# Covers term/daily transitions, lottery auto-delete, cash-sale
# combo packets, past-due event cleanup, overlap validation.
add_executable(test_tcontrolsvr_asio_event_scheduler
    ${TCONTROL_ASIO_SRC}/tests/test_event_scheduler.cpp)
target_link_libraries(test_tcontrolsvr_asio_event_scheduler PRIVATE
    tcontrolsvr_asio_core)
add_test(NAME tcontrolsvr_asio_event_scheduler
    COMMAND test_tcontrolsvr_asio_event_scheduler)

# F5 wire test: patch metadata SP wiring (CT_UPDATEPATCH_REQ +
# CT_PREVERSIONTABLE/UPDATE_REQ) + authority gate on those handlers.
add_executable(test_tcontrolsvr_asio_patch_metadata
    ${TCONTROL_ASIO_SRC}/tests/test_patch_metadata.cpp)
target_link_libraries(test_tcontrolsvr_asio_patch_metadata PRIVATE
    tcontrolsvr_asio_core)
add_test(NAME tcontrolsvr_asio_patch_metadata
    COMMAND test_tcontrolsvr_asio_patch_metadata)

# Round-2 wire-parity regression suite: locks down the four
# count-width mismatches (CHATBANLIST / EVENTLIST / CASHITEMLIST /
# PREVERSIONTABLE), the CHATBANLIST field order, the EVENTUPDATE_REQ
# EventInfo body append, plus smoke coverage for the previously-
# missing handlers (MONACTION_REQ, ITEMFIND_REQ, SERVICEDATACLEAR_REQ).
add_executable(test_tcontrolsvr_asio_wire_parity
    ${TCONTROL_ASIO_SRC}/tests/test_wire_parity.cpp)
target_link_libraries(test_tcontrolsvr_asio_wire_parity PRIVATE
    tcontrolsvr_asio_core)
add_test(NAME tcontrolsvr_asio_wire_parity
    COMMAND test_tcontrolsvr_asio_wire_parity)

# SOCI integration suite: exercises the five SOCI repositories
# against a live database. Skips cleanly when neither
# TCONTROLSVR_TEST_PG_CONN nor TCONTROLSVR_TEST_MSSQL_CONN is in
# the environment, matching the convention from
# Server/TLoginSvrAsio/tests and Server/TPatchSvrAsio/tests.
add_executable(test_tcontrolsvr_asio_soci_repositories
    ${TCONTROL_ASIO_SRC}/tests/test_soci_repositories.cpp)
target_link_libraries(test_tcontrolsvr_asio_soci_repositories PRIVATE
    tcontrolsvr_asio_core)
add_test(NAME tcontrolsvr_asio_soci_repositories
    COMMAND test_tcontrolsvr_asio_soci_repositories)

# CoOffload helper test — verifies the lambda runs on a worker
# thread, the coroutine resumes on the io_context, and exceptions
# propagate. Pure logic, no DB.
add_executable(test_tcontrolsvr_asio_co_offload
    ${TCONTROL_ASIO_SRC}/tests/test_co_offload.cpp)
target_link_libraries(test_tcontrolsvr_asio_co_offload PRIVATE
    tcontrolsvr_asio_core)
add_test(NAME tcontrolsvr_asio_co_offload
    COMMAND test_tcontrolsvr_asio_co_offload)
