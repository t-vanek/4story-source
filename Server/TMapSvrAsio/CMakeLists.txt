# TMapSvrAsio — modernized gameplay server, runs alongside the legacy
# TMapSvr.exe. Phased migration plan in this directory's README; F1
# ships the scaffold + CS_CONNECT_REQ handshake only.

set(TMAPSVR_ASIO_SRC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(spdlog QUIET)
find_package(tomlplusplus QUIET)
find_package(SOCI CONFIG QUIET)

# Pick whichever SOCI flavor (static vs shared) vcpkg ships locally —
# same probe as the other Asio servers' CMake files.
set(TMAPSVR_SOCI_LIBS "")
if(SOCI_FOUND)
    if(TARGET SOCI::soci_core)
        list(APPEND TMAPSVR_SOCI_LIBS SOCI::soci_core)
    elseif(TARGET SOCI::soci_core_static)
        list(APPEND TMAPSVR_SOCI_LIBS SOCI::soci_core_static)
    endif()
    if(TARGET SOCI::soci_odbc)
        list(APPEND TMAPSVR_SOCI_LIBS SOCI::soci_odbc)
    elseif(TARGET SOCI::soci_odbc_static)
        list(APPEND TMAPSVR_SOCI_LIBS SOCI::soci_odbc_static)
    endif()
    if(TARGET SOCI::soci_sqlite3)
        list(APPEND TMAPSVR_SOCI_LIBS SOCI::soci_sqlite3)
    elseif(TARGET SOCI::soci_sqlite3_static)
        list(APPEND TMAPSVR_SOCI_LIBS SOCI::soci_sqlite3_static)
    endif()
    if(TARGET SOCI::soci_postgresql)
        list(APPEND TMAPSVR_SOCI_LIBS SOCI::soci_postgresql)
    endif()
else()
    list(APPEND TMAPSVR_SOCI_LIBS soci_sqlite3 soci_odbc soci_core dl)
endif()

# Shared static library of the server's logic — keeps the entry-point
# main.cpp thin and lets the integration test instantiate MapServer
# in-process instead of forking the binary.
add_library(tmapsvr_asio_core STATIC
    ${TMAPSVR_ASIO_SRC}/map_server.cpp
    ${TMAPSVR_ASIO_SRC}/handlers.cpp
    ${TMAPSVR_ASIO_SRC}/handlers_map.cpp
    ${TMAPSVR_ASIO_SRC}/handlers_world.cpp
    ${TMAPSVR_ASIO_SRC}/handlers_combat.cpp
    ${TMAPSVR_ASIO_SRC}/config.cpp
    ${TMAPSVR_ASIO_SRC}/db/schema_validator.cpp
    ${TMAPSVR_ASIO_SRC}/services/soci_session_validator.cpp
    ${TMAPSVR_ASIO_SRC}/services/soci_player_service.cpp
    ${TMAPSVR_ASIO_SRC}/services/world_client.cpp)
target_include_directories(tmapsvr_asio_core PUBLIC
    ${TMAPSVR_ASIO_SRC}
    ${CMAKE_SOURCE_DIR}/Lib/Own/TNetLib/TNetLib
    ${FOURSTORY_TPROTOCOL_INCLUDE_DIR})
target_link_libraries(tmapsvr_asio_core PUBLIC
    tnetlib_portable fourstory_common ${TMAPSVR_SOCI_LIBS})

if(spdlog_FOUND)
    target_link_libraries(tmapsvr_asio_core PUBLIC spdlog::spdlog)
else()
    target_link_libraries(tmapsvr_asio_core PUBLIC spdlog)
endif()
if(tomlplusplus_FOUND)
    target_link_libraries(tmapsvr_asio_core PUBLIC tomlplusplus::tomlplusplus)
endif()
# Else: toml++ is header-only on Debian/Ubuntu — no link step needed.

set_target_properties(tmapsvr_asio_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

# Entry-point binary.
add_executable(tmapsvr_asio ${TMAPSVR_ASIO_SRC}/main.cpp)
target_link_libraries(tmapsvr_asio PRIVATE tmapsvr_asio_core)
fourstory_deploy_ossl_legacy(tmapsvr_asio)

# Characterization spec tests — one binary per legacy handler. Each
# test file documents the legacy branches (with file:line citations)
# and asserts the modern impl matches. Pending branches are logged but
# don't fail the run. See `tests/LEGACY_SPEC.md` for the methodology.
add_executable(test_tmapsvr_asio_cs_connect_req_spec
    ${TMAPSVR_ASIO_SRC}/tests/test_cs_connect_req_spec.cpp)
target_link_libraries(test_tmapsvr_asio_cs_connect_req_spec PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_cs_connect_req_spec
    COMMAND test_tmapsvr_asio_cs_connect_req_spec)
fourstory_deploy_ossl_legacy(test_tmapsvr_asio_cs_connect_req_spec)

add_executable(test_tmapsvr_asio_cs_terminate_req_spec
    ${TMAPSVR_ASIO_SRC}/tests/test_cs_terminate_req_spec.cpp)
target_link_libraries(test_tmapsvr_asio_cs_terminate_req_spec PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_cs_terminate_req_spec
    COMMAND test_tmapsvr_asio_cs_terminate_req_spec)
fourstory_deploy_ossl_legacy(test_tmapsvr_asio_cs_terminate_req_spec)

add_executable(test_tmapsvr_asio_cs_conready_req_spec
    ${TMAPSVR_ASIO_SRC}/tests/test_cs_conready_req_spec.cpp)
target_link_libraries(test_tmapsvr_asio_cs_conready_req_spec PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_cs_conready_req_spec
    COMMAND test_tmapsvr_asio_cs_conready_req_spec)
fourstory_deploy_ossl_legacy(test_tmapsvr_asio_cs_conready_req_spec)

add_executable(test_tmapsvr_asio_cs_kickout_req_spec
    ${TMAPSVR_ASIO_SRC}/tests/test_cs_kickout_req_spec.cpp)
target_link_libraries(test_tmapsvr_asio_cs_kickout_req_spec PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_cs_kickout_req_spec
    COMMAND test_tmapsvr_asio_cs_kickout_req_spec)
fourstory_deploy_ossl_legacy(test_tmapsvr_asio_cs_kickout_req_spec)

add_executable(test_tmapsvr_asio_sm_quitservice_req_spec
    ${TMAPSVR_ASIO_SRC}/tests/test_sm_quitservice_req_spec.cpp)
target_link_libraries(test_tmapsvr_asio_sm_quitservice_req_spec PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_sm_quitservice_req_spec
    COMMAND test_tmapsvr_asio_sm_quitservice_req_spec)
fourstory_deploy_ossl_legacy(test_tmapsvr_asio_sm_quitservice_req_spec)

add_executable(test_tmapsvr_asio_cs_winldic_req_spec
    ${TMAPSVR_ASIO_SRC}/tests/test_cs_winldic_req_spec.cpp)
target_link_libraries(test_tmapsvr_asio_cs_winldic_req_spec PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_cs_winldic_req_spec
    COMMAND test_tmapsvr_asio_cs_winldic_req_spec)
fourstory_deploy_ossl_legacy(test_tmapsvr_asio_cs_winldic_req_spec)

add_executable(test_tmapsvr_asio_ct_servicemonitor_ack_spec
    ${TMAPSVR_ASIO_SRC}/tests/test_ct_servicemonitor_ack_spec.cpp)
target_link_libraries(test_tmapsvr_asio_ct_servicemonitor_ack_spec PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_ct_servicemonitor_ack_spec
    COMMAND test_tmapsvr_asio_ct_servicemonitor_ack_spec)
fourstory_deploy_ossl_legacy(test_tmapsvr_asio_ct_servicemonitor_ack_spec)

# Batch: more handlers with short legacy bodies — CT_* operator
# protocol + CS_ARENARANKING_REQ. See tests/LEGACY_SPEC.md.
foreach(_spec
        ct_ctrlsvr_req
        ct_servicedataclear_ack
        ct_announcement_ack
        ct_userkickout_ack
        cs_arenaranking_req
        cs_disconnect_req
        cs_verifysession_ack
        cs_pingmeasurement_req
        cs_registerbow_req
        cs_cancelbowqueue_req
        cs_chgmode_req
        cs_chgchannel_req
        cs_corpsask_req
        cs_herolist_req
        cs_petcancel_req)
    add_executable(test_tmapsvr_asio_${_spec}_spec
        ${TMAPSVR_ASIO_SRC}/tests/test_${_spec}_spec.cpp)
    target_link_libraries(test_tmapsvr_asio_${_spec}_spec PRIVATE
        tmapsvr_asio_core)
    add_test(NAME tmapsvr_asio_${_spec}_spec
        COMMAND test_tmapsvr_asio_${_spec}_spec)
    fourstory_deploy_ossl_legacy(test_tmapsvr_asio_${_spec}_spec)
endforeach()

# Bottom-up legacy port — per-layer spec tests. Pure-unit, no Asio /
# sockets needed. Layer N depends on layer N-1; tests run
# independently so a layer-1 regression surfaces before layer-2
# breakage.
# Layer 1: trivially-copyable PODs (types.h)
# Layer 2: string-bearing compound types (types_layer2.h)
# Layer 3: container-bearing aggregate types (types_layer3.h)
foreach(_layer 1 2 3)
    add_executable(test_tmapsvr_asio_legacy_port_layer${_layer}_spec
        ${TMAPSVR_ASIO_SRC}/tests/test_legacy_port_layer${_layer}_spec.cpp)
    target_include_directories(test_tmapsvr_asio_legacy_port_layer${_layer}_spec
        PRIVATE ${TMAPSVR_ASIO_SRC})
    add_test(NAME tmapsvr_asio_legacy_port_layer${_layer}_spec
        COMMAND test_tmapsvr_asio_legacy_port_layer${_layer}_spec)
endforeach()

# F2b: IPlayerService + FakePlayerService — pure in-memory, no SOCI.
add_executable(test_tmapsvr_asio_player_service_spec
    ${TMAPSVR_ASIO_SRC}/tests/test_player_service_spec.cpp)
target_include_directories(test_tmapsvr_asio_player_service_spec
    PRIVATE ${TMAPSVR_ASIO_SRC})
add_test(NAME tmapsvr_asio_player_service_spec
    COMMAND test_tmapsvr_asio_player_service_spec)

# F2 integration test — SOCI-backed session validator against a live DB.
# Skips when neither TMAPSVR_TEST_PG_CONN nor TMAPSVR_TEST_MSSQL_CONN
# is set, so a no-DB CI run still passes.
add_executable(test_tmapsvr_asio_soci_session_validator
    ${TMAPSVR_ASIO_SRC}/tests/test_soci_session_validator.cpp)
target_link_libraries(test_tmapsvr_asio_soci_session_validator PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_soci_session_validator
    COMMAND test_tmapsvr_asio_soci_session_validator)
fourstory_deploy_ossl_legacy(test_tmapsvr_asio_soci_session_validator)

# F2b Part 3 spec test — OnDmLoadCharReq + FakeWorldClient (pure-unit, no sockets).
add_executable(test_tmapsvr_asio_dm_loadchar_req_spec
    ${TMAPSVR_ASIO_SRC}/tests/test_dm_loadchar_req_spec.cpp)
target_link_libraries(test_tmapsvr_asio_dm_loadchar_req_spec PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_dm_loadchar_req_spec
    COMMAND test_tmapsvr_asio_dm_loadchar_req_spec)
fourstory_deploy_ossl_legacy(test_tmapsvr_asio_dm_loadchar_req_spec)

# F4 Part 3 — LocalPlayerHpRegistry spec (pure-unit).
add_executable(test_tmapsvr_asio_player_hp_registry_spec
    ${TMAPSVR_ASIO_SRC}/tests/test_player_hp_registry_spec.cpp)
target_include_directories(test_tmapsvr_asio_player_hp_registry_spec
    PRIVATE ${TMAPSVR_ASIO_SRC}
            ${CMAKE_SOURCE_DIR}/Lib/Own/TNetLib/TNetLib)
add_test(NAME tmapsvr_asio_player_hp_registry_spec
    COMMAND test_tmapsvr_asio_player_hp_registry_spec)

# F4 Part 3 — CS_DEFEND_REQ characterization spec.
add_executable(test_tmapsvr_asio_cs_defend_req_spec
    ${TMAPSVR_ASIO_SRC}/tests/test_cs_defend_req_spec.cpp)
target_link_libraries(test_tmapsvr_asio_cs_defend_req_spec PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_cs_defend_req_spec
    COMMAND test_tmapsvr_asio_cs_defend_req_spec)
fourstory_deploy_ossl_legacy(test_tmapsvr_asio_cs_defend_req_spec)

# F4b — HardcodedLevelChart spec (pure-unit, no Asio).
add_executable(test_tmapsvr_asio_level_chart_spec
    ${TMAPSVR_ASIO_SRC}/tests/test_level_chart_spec.cpp)
target_include_directories(test_tmapsvr_asio_level_chart_spec
    PRIVATE ${TMAPSVR_ASIO_SRC}
            ${CMAKE_SOURCE_DIR}/Lib/Own/TNetLib/TNetLib)
add_test(NAME tmapsvr_asio_level_chart_spec
    COMMAND test_tmapsvr_asio_level_chart_spec)

# F4b — LocalSpawnManager spec (needs io_context).
add_executable(test_tmapsvr_asio_spawn_manager_spec
    ${TMAPSVR_ASIO_SRC}/tests/test_spawn_manager_spec.cpp)
target_link_libraries(test_tmapsvr_asio_spawn_manager_spec PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_spawn_manager_spec
    COMMAND test_tmapsvr_asio_spawn_manager_spec)
fourstory_deploy_ossl_legacy(test_tmapsvr_asio_spawn_manager_spec)

# F4 — LocalMonsterRegistry spec (pure-unit).
add_executable(test_tmapsvr_asio_monster_state_spec
    ${TMAPSVR_ASIO_SRC}/tests/test_monster_state_spec.cpp)
target_include_directories(test_tmapsvr_asio_monster_state_spec
    PRIVATE ${TMAPSVR_ASIO_SRC}
            ${CMAKE_SOURCE_DIR}/Lib/Own/TNetLib/TNetLib)
add_test(NAME tmapsvr_asio_monster_state_spec
    COMMAND test_tmapsvr_asio_monster_state_spec)

# F4 — CS_ACTION_REQ characterization spec.
add_executable(test_tmapsvr_asio_cs_action_req_spec
    ${TMAPSVR_ASIO_SRC}/tests/test_cs_action_req_spec.cpp)
target_link_libraries(test_tmapsvr_asio_cs_action_req_spec PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_cs_action_req_spec
    COMMAND test_tmapsvr_asio_cs_action_req_spec)
fourstory_deploy_ossl_legacy(test_tmapsvr_asio_cs_action_req_spec)

# F3 — CS_MOVE_REQ characterization spec (pure-unit, no wire sockets).
add_executable(test_tmapsvr_asio_cs_move_req_spec
    ${TMAPSVR_ASIO_SRC}/tests/test_cs_move_req_spec.cpp)
target_link_libraries(test_tmapsvr_asio_cs_move_req_spec PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_cs_move_req_spec
    COMMAND test_tmapsvr_asio_cs_move_req_spec)
fourstory_deploy_ossl_legacy(test_tmapsvr_asio_cs_move_req_spec)

# F3 — LocalMapState AOI spec test (pure-unit, no sockets or Asio).
add_executable(test_tmapsvr_asio_map_state_spec
    ${TMAPSVR_ASIO_SRC}/tests/test_map_state_spec.cpp)
target_include_directories(test_tmapsvr_asio_map_state_spec
    PRIVATE ${TMAPSVR_ASIO_SRC}
            ${CMAKE_SOURCE_DIR}/Lib/Own/TNetLib/TNetLib)
add_test(NAME tmapsvr_asio_map_state_spec
    COMMAND test_tmapsvr_asio_map_state_spec)

# F2b Part 4 spec test — pre-load cache + pending session + MW_CONRESULT.
add_executable(test_tmapsvr_asio_mw_conresult_req_spec
    ${TMAPSVR_ASIO_SRC}/tests/test_mw_conresult_req_spec.cpp)
target_link_libraries(test_tmapsvr_asio_mw_conresult_req_spec PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_mw_conresult_req_spec
    COMMAND test_tmapsvr_asio_mw_conresult_req_spec)
fourstory_deploy_ossl_legacy(test_tmapsvr_asio_mw_conresult_req_spec)

# F2b integration test — SOCI-backed player service against a live DB.
# Skips when neither TMAPSVR_TEST_PG_CONN nor TMAPSVR_TEST_MSSQL_CONN
# is set. Set TMAPSVR_TEST_CHAR_ID + TMAPSVR_TEST_USER_ID env vars to
# target a specific TCHARTABLE row (default: both = 1).
add_executable(test_tmapsvr_asio_soci_player_service
    ${TMAPSVR_ASIO_SRC}/tests/test_soci_player_service.cpp)
target_link_libraries(test_tmapsvr_asio_soci_player_service PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_soci_player_service
    COMMAND test_tmapsvr_asio_soci_player_service)
fourstory_deploy_ossl_legacy(test_tmapsvr_asio_soci_player_service)
