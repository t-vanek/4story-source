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
    ${TCONTROL_ASIO_SRC}/admin_shell.cpp
    ${TCONTROL_ASIO_SRC}/config.cpp
    ${TCONTROL_ASIO_SRC}/control_session.cpp
    ${TCONTROL_ASIO_SRC}/control_server.cpp
    ${TCONTROL_ASIO_SRC}/event_codec.cpp
    ${TCONTROL_ASIO_SRC}/event_scheduler.cpp
    ${TCONTROL_ASIO_SRC}/message_router.cpp
    ${TCONTROL_ASIO_SRC}/peer_dialer.cpp
    ${TCONTROL_ASIO_SRC}/senders.cpp
    ${TCONTROL_ASIO_SRC}/handlers/handlers_auth.cpp
    ${TCONTROL_ASIO_SRC}/handlers/handlers_service.cpp
    ${TCONTROL_ASIO_SRC}/handlers/handlers_admin.cpp
    ${TCONTROL_ASIO_SRC}/handlers/handlers_event.cpp
    ${TCONTROL_ASIO_SRC}/handlers/handlers_extra.cpp
    ${TCONTROL_ASIO_SRC}/handlers/handlers_patch.cpp
    ${TCONTROL_ASIO_SRC}/handlers/handlers_registry.cpp
    ${TCONTROL_ASIO_SRC}/db/schema_validator.cpp
    ${TCONTROL_ASIO_SRC}/services/chat_ban_repository.cpp
    ${TCONTROL_ASIO_SRC}/services/event_registry.cpp
    ${TCONTROL_ASIO_SRC}/services/fake_operator_auth_service.cpp
    ${TCONTROL_ASIO_SRC}/services/operator_registry.cpp
    ${TCONTROL_ASIO_SRC}/services/peer_registry.cpp
    ${TCONTROL_ASIO_SRC}/services/registry_event_bus.cpp
    ${TCONTROL_ASIO_SRC}/services/soci_alerter.cpp
    ${TCONTROL_ASIO_SRC}/services/soci_event_repository.cpp
    ${TCONTROL_ASIO_SRC}/services/soci_operator_auth_service.cpp
    ${TCONTROL_ASIO_SRC}/services/soci_patch_metadata_service.cpp
    ${TCONTROL_ASIO_SRC}/services/soci_admin_audit_logger.cpp
    ${TCONTROL_ASIO_SRC}/services/soci_peer_repository.cpp
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

# AdminShell test — drives every command through DispatchForTest
# against fake audit + controller + a real PeerRegistry, plus loopback
# peer sockets for the fan-out commands so the test reads the actual
# CT_USERKICKOUT_ACK / CT_ANNOUNCEMENT_ACK bytes off the wire.
add_executable(test_tcontrolsvr_asio_admin_shell
    ${TCONTROL_ASIO_SRC}/tests/test_admin_shell.cpp)
target_link_libraries(test_tcontrolsvr_asio_admin_shell PRIVATE
    tcontrolsvr_asio_core)
add_test(NAME tcontrolsvr_asio_admin_shell
    COMMAND test_tcontrolsvr_asio_admin_shell)

# Peer self-registration (F1 of the cluster control plane): unit
# coverage of PeerRegistry's lease semantics + wire coverage of the
# CT_PEER_REGISTER / HEARTBEAT / DEREGISTER handlers against a
# loopback OperatorSession. No DB.
add_executable(test_tcontrolsvr_asio_peer_registry
    ${TCONTROL_ASIO_SRC}/tests/test_peer_registry.cpp)
target_link_libraries(test_tcontrolsvr_asio_peer_registry PRIVATE
    tcontrolsvr_asio_core)
add_test(NAME tcontrolsvr_asio_peer_registry
    COMMAND test_tcontrolsvr_asio_peer_registry)

# Cluster orchestration (F6) — drives `cluster start`, `cluster stop`,
# `cluster restart`, `cluster wait-healthy` against a counting fake
# controller + a real PeerRegistry mutated from helper threads (to
# simulate peers registering / deregistering during the operation).
add_executable(test_tcontrolsvr_asio_admin_shell_cluster
    ${TCONTROL_ASIO_SRC}/tests/test_admin_shell_cluster.cpp)
target_link_libraries(test_tcontrolsvr_asio_admin_shell_cluster PRIVATE
    tcontrolsvr_asio_core)
add_test(NAME tcontrolsvr_asio_admin_shell_cluster
    COMMAND test_tcontrolsvr_asio_admin_shell_cluster)

# Admin-shell streaming subscription (F5) — drives `subscribe
# registry` over a real loopback connection, triggers Register /
# Heartbeat / Deregister / ExpireStale on PeerRegistry from the test
# thread, and asserts the matching registry.* lines arrive on the
# client side via the event bus + writer coroutine.
add_executable(test_tcontrolsvr_asio_admin_shell_stream
    ${TCONTROL_ASIO_SRC}/tests/test_admin_shell_stream.cpp)
target_link_libraries(test_tcontrolsvr_asio_admin_shell_stream PRIVATE
    tcontrolsvr_asio_core)
add_test(NAME tcontrolsvr_asio_admin_shell_stream
    COMMAND test_tcontrolsvr_asio_admin_shell_stream)

# Admin-shell gateway commands (F4) — drives the new `route` +
# `peer <sid>` commands through DispatchForTest with loopback peer
# sockets behind the registry, then reads bytes off the wire to verify
# the gateway is byte-for-byte routing operator CLI input through the
# F3 MessageRouter to the right destination(s).
add_executable(test_tcontrolsvr_asio_admin_shell_route
    ${TCONTROL_ASIO_SRC}/tests/test_admin_shell_route.cpp)
target_link_libraries(test_tcontrolsvr_asio_admin_shell_route PRIVATE
    tcontrolsvr_asio_core)
add_test(NAME tcontrolsvr_asio_admin_shell_route
    COMMAND test_tcontrolsvr_asio_admin_shell_route)

# MessageRouter routing primitives — covers all four routing methods
# (SendToService, SendToType round-robin, BroadcastToGroupType,
# BroadcastToType) against loopback peer sockets. Verifies the right
# peers received the right bytes and that offline / closed peers are
# skipped without breaking round-robin.
add_executable(test_tcontrolsvr_asio_message_router
    ${TCONTROL_ASIO_SRC}/tests/test_message_router.cpp)
target_link_libraries(test_tcontrolsvr_asio_message_router PRIVATE
    tcontrolsvr_asio_core)
add_test(NAME tcontrolsvr_asio_message_router
    COMMAND test_tcontrolsvr_asio_message_router)

# PeerClient integration test — drives the outbound-side client
# (fourstory::cluster::PeerClient) against a TCP stub that role-plays
# TControl's CT_PEER_* wire endpoint. Verifies REGISTER + HEARTBEAT
# + DEREGISTER round-trips and the rejection/reconnect paths.
add_executable(test_tcontrolsvr_asio_peer_client
    ${TCONTROL_ASIO_SRC}/tests/test_peer_client.cpp)
target_link_libraries(test_tcontrolsvr_asio_peer_client PRIVATE
    tcontrolsvr_asio_core)
add_test(NAME tcontrolsvr_asio_peer_client
    COMMAND test_tcontrolsvr_asio_peer_client)

# ControlSession TLS round-trip — wraps the accepted socket in
# asio::ssl::stream, drives the server-side handshake, and verifies
# the wire protocol (8-byte header + checksum + body) flows through
# the encrypted channel. Reuses the tnetlib cert fixtures.
set(CONTROL_TLS_CERT_DIR
    ${CMAKE_BINARY_DIR}/Lib/Own/TNetLib/test_certs)
if(EXISTS ${CONTROL_TLS_CERT_DIR}/ca.crt)
    add_executable(test_tcontrolsvr_asio_control_session_tls
        ${TCONTROL_ASIO_SRC}/tests/test_control_session_tls.cpp)
    target_link_libraries(test_tcontrolsvr_asio_control_session_tls
        PRIVATE tcontrolsvr_asio_core)
    target_compile_definitions(test_tcontrolsvr_asio_control_session_tls
        PRIVATE TEST_CERT_DIR="${CONTROL_TLS_CERT_DIR}")
    add_test(NAME tcontrolsvr_asio_control_session_tls
        COMMAND test_tcontrolsvr_asio_control_session_tls)

    # Hybrid first-byte detection — TLS clients (0x16 ClientHello) and
    # legacy plain-TCP operators on the SAME listener. Exercises the
    # Phase A.2 peek branch in ControlServer::Run().
    add_executable(test_tcontrolsvr_asio_hybrid
        ${TCONTROL_ASIO_SRC}/tests/test_control_server_hybrid.cpp)
    target_link_libraries(test_tcontrolsvr_asio_hybrid
        PRIVATE tcontrolsvr_asio_core)
    target_compile_definitions(test_tcontrolsvr_asio_hybrid
        PRIVATE TEST_CERT_DIR="${CONTROL_TLS_CERT_DIR}")
    add_test(NAME tcontrolsvr_asio_hybrid
        COMMAND test_tcontrolsvr_asio_hybrid)

    # CN → TPEER_AUTH.peer_name validation on CT_PEER_REGISTER_REQ.
    # Verifies the register handler rejects a peer whose TLS cert CN
    # doesn't match the operator-configured peer_name in the trust
    # map; accepts when they match; and skips the check entirely when
    # the gate has no opinion on the identity.
    add_executable(test_tcontrolsvr_asio_register_cn
        ${TCONTROL_ASIO_SRC}/tests/test_register_cn_validation.cpp)
    target_link_libraries(test_tcontrolsvr_asio_register_cn
        PRIVATE tcontrolsvr_asio_core)
    target_compile_definitions(test_tcontrolsvr_asio_register_cn
        PRIVATE TEST_CERT_DIR="${CONTROL_TLS_CERT_DIR}")
    add_test(NAME tcontrolsvr_asio_register_cn
        COMMAND test_tcontrolsvr_asio_register_cn)
endif()

# CoOffload helper test — verifies the lambda runs on a worker
# thread, the coroutine resumes on the io_context, and exceptions
# propagate. Pure logic, no DB. Also covers the CoOffloadIf /
# CoOffloadVoidIf optional-pool variants the handlers use.
add_executable(test_tcontrolsvr_asio_co_offload
    ${TCONTROL_ASIO_SRC}/tests/test_co_offload.cpp)
target_link_libraries(test_tcontrolsvr_asio_co_offload PRIVATE
    tcontrolsvr_asio_core)
add_test(NAME tcontrolsvr_asio_co_offload
    COMMAND test_tcontrolsvr_asio_co_offload)

# RegistryRefresher coroutine-hook test — verifies the new
# AddCoroutineHook path the inventory-refresh tick uses (sync hooks,
# coroutine hooks, both together, exception isolation). Pure timer
# loop, no DB.
add_executable(test_tcontrolsvr_asio_registry_refresher
    ${TCONTROL_ASIO_SRC}/tests/test_registry_refresher.cpp)
target_link_libraries(test_tcontrolsvr_asio_registry_refresher PRIVATE
    tcontrolsvr_asio_core)
add_test(NAME tcontrolsvr_asio_registry_refresher
    COMMAND test_tcontrolsvr_asio_registry_refresher)
