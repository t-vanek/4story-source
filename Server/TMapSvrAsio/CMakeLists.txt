# TMapSvrAsio — modernized 4Story map (gameplay) server. Work in
# progress, port-in-progress from the legacy Win32 / MFC / ATL
# Server/TMapSvr/ implementation, modeled on the other modernized
# Asio servers (TLoginSvrAsio, TControlSvrAsio, TPatchSvrAsio,
# TLogSvrAsio): Boost.Asio coroutines, spdlog, toml++, SOCI/ODBC,
# AsioSession from Lib/Own/TNetLib, MessageId from Lib/Own/TProtocol.
#
# Layout
# ------
#   main.cpp          F7 entry point (config + pool + validator +
#                     session registry + channel presence + world
#                     client w/ inbound dispatch + listener).
#   config.h/cpp      TOML loader — port, mode, [crypto], [database],
#                     [world].
#   tmapsvr.example.toml  Documented defaults.
#   map_server.h/cpp  Accept loop, per-connection coroutine, RC4 toggle,
#                     session-registry + presence teardown.
#   handlers.h        Common types (HandlerContext) + handler
#                     declarations for all CS_* / CT_* messages.
#   handlers/         Per-area implementation files (T1 split):
#                       common.h     SenderCharId helper (hdr-only)
#                       dispatch.cpp central switch over MessageId
#                       session.cpp  CONNECT_REQ, CONREADY_REQ
#                       movement.cpp MOVE_REQ + AOI broadcast
#                       npc.cpp      NPCTALK_REQ
#                       skill.cpp    SKILLUSE_REQ
#                       quest.cpp    QUESTEXEC_REQ, QUESTDROP_REQ
#                       social.cpp   CHAT_REQ + 3× PARTY*_REQ
#                       bow.cpp      3× Bow/BR mode handlers
#                       control.cpp  5× CT_* control stubs
#   handlers_world.h/cpp
#                     Inbound world dispatch (DM_LOADCHAR_REQ stub —
#                     full char load lands with the F8 player service).
#   wire_codec.h      POD reader/writer for legacy CPacket body
#                     layout — shared with handlers + tests.
#   services/         Service interfaces + impls (session validator,
#                     session registry, channel presence, world client,
#                     player loader, …). One header per interface +
#                     paired soci_/fake_/in_memory_ impls.
#   db/               Per-server schema validators (boot-time fail-fast).
#   legacy_src/       Verbatim copy of Server/TMapSvr/, reference only,
#                     not compiled.
#
# Phase plan: F1 main scaffold (done), F2 config + schema validator
# (done), F3 MapServer accept loop (done), F4 CS_CONNECT_REQ +
# session validator (done), F5 World peer + MW_ADDCHAR_ACK (done),
# F6 inbound world dispatch + session registry (done), F7 movement
# + flat AOI broadcast (done), F8 player service + DM_LOADCHAR_ACK
# (done), F9 inventory service + section (done), F10 NPC chart +
# CS_NPCTALK_REQ (done), F11 per-char skill list + section (done),
# F12 per-char quest progress + section (done), F13 monster +
# spawn charts + monster registry (done), F14 chat + party decode
# stubs (done), F15 companion service + section (done), F16 BR/Bow
# run-time mode handlers (done), F17 CT_* control stubs + /healthz
# (this commit — closes the scaffolding sweep).
#
# Consolidation TODO inventory (post-F17):
#   F4 — wVersion check + llChecksum verification (legacy formula)
#   F5 — WorldClient send queue / strand for multi-thread io
#   F7 — speed-hack check, ghost-mode handling, CS_CONREADY enter-
#        map flood
#   F8 — trailing DM_LOADCHAR_ACK sections (secure code, aid table,
#        PC bang, post info, cabinet, equip, friends, mail, chapter,
#        pet, recall-mon)
#   F10 — country / disguise gate, shop / CS_NPCITEMLIST_REQ flow
#   F11 — skill-template chart (TSKILLCHART) + damage calculation +
#         CS_SKILLUSE_ACK broadcast
#   F12 — quest-engine evaluation (18+ trigger types) + reward award
#   F13 — SpawnManager + AI tick (timer-driven monster lifecycle)
#   F14 — chat broadcast + party world-relay encoders
#   F15 — TPETTABLE / TRECALLMONTABLE sections + summon handlers
#   F16 — MW_ADDTOBOWQUEUE relay + medal accounting + Revival call
#   F17 — peer-IP auth gate for CT_*, UDP audit shim to TLogSvr,
#         admin TCP shell, CT_ANNOUNCEMENT broadcast, CT_USERKICKOUT
#         session close, stale-client sweep on CT_SERVICEMONITOR_ACK

set(TMAPSVR_ASIO_SRC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(spdlog QUIET)
find_package(tomlplusplus QUIET)
find_package(SOCI CONFIG QUIET)

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
else()
    list(APPEND TMAPSVR_SOCI_LIBS soci_odbc soci_core dl)
endif()

# Core static library — config, schema validator, and (from F3 on)
# the MapServer / handlers / services layer. Split from the executable
# so unit tests can link against the core without dragging in main().
add_library(tmapsvr_asio_core STATIC
    ${TMAPSVR_ASIO_SRC}/audit/audit_log.cpp
    ${TMAPSVR_ASIO_SRC}/config.cpp
    ${TMAPSVR_ASIO_SRC}/map_server.cpp
    ${TMAPSVR_ASIO_SRC}/ops/admin_shell.cpp
    ${TMAPSVR_ASIO_SRC}/ops/metrics.cpp
    ${TMAPSVR_ASIO_SRC}/ops/metrics_endpoint.cpp
    ${TMAPSVR_ASIO_SRC}/handlers_world.cpp
    ${TMAPSVR_ASIO_SRC}/handlers/dispatch.cpp
    ${TMAPSVR_ASIO_SRC}/handlers/session.cpp
    ${TMAPSVR_ASIO_SRC}/handlers/movement.cpp
    ${TMAPSVR_ASIO_SRC}/handlers/npc.cpp
    ${TMAPSVR_ASIO_SRC}/handlers/skill.cpp
    ${TMAPSVR_ASIO_SRC}/handlers/quest.cpp
    ${TMAPSVR_ASIO_SRC}/handlers/social.cpp
    ${TMAPSVR_ASIO_SRC}/handlers/bow.cpp
    ${TMAPSVR_ASIO_SRC}/handlers/control.cpp
    ${TMAPSVR_ASIO_SRC}/services/soci_session_validator.cpp
    ${TMAPSVR_ASIO_SRC}/services/log_peer.cpp
    ${TMAPSVR_ASIO_SRC}/services/rate_limiter.cpp
    ${TMAPSVR_ASIO_SRC}/services/soci_companion_service.cpp
    ${TMAPSVR_ASIO_SRC}/services/soci_inventory_service.cpp
    ${TMAPSVR_ASIO_SRC}/services/soci_monster_chart.cpp
    ${TMAPSVR_ASIO_SRC}/services/soci_npc_service.cpp
    ${TMAPSVR_ASIO_SRC}/services/soci_player_service.cpp
    ${TMAPSVR_ASIO_SRC}/services/soci_quest_service.cpp
    ${TMAPSVR_ASIO_SRC}/services/soci_skill_service.cpp
    ${TMAPSVR_ASIO_SRC}/services/soci_spawn_chart.cpp
    ${TMAPSVR_ASIO_SRC}/services/world_client.cpp
    ${TMAPSVR_ASIO_SRC}/services/world_senders.cpp
    ${TMAPSVR_ASIO_SRC}/services/client_senders.cpp
    ${TMAPSVR_ASIO_SRC}/db/schema_validator.cpp)

target_include_directories(tmapsvr_asio_core PUBLIC
    ${TMAPSVR_ASIO_SRC})

# tnetlib_portable carries AsioSession (RC4+XOR+sequence wire codec)
# and the MessageId enum the F4+ handlers dispatch on.
target_link_libraries(tmapsvr_asio_core PUBLIC
    fourstory_common
    tnetlib_portable
    ${TMAPSVR_SOCI_LIBS})

if(TARGET Boost::system)
    target_link_libraries(tmapsvr_asio_core PUBLIC Boost::system)
endif()
if(spdlog_FOUND)
    target_link_libraries(tmapsvr_asio_core PUBLIC spdlog::spdlog)
else()
    target_link_libraries(tmapsvr_asio_core PUBLIC spdlog)
endif()
if(tomlplusplus_FOUND)
    target_link_libraries(tmapsvr_asio_core PUBLIC tomlplusplus::tomlplusplus)
endif()

set_target_properties(tmapsvr_asio_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Executable entry point. Just main.cpp + link against the core; the
# rest of the wiring lives inside the static library.
add_executable(tmapsvr_asio ${TMAPSVR_ASIO_SRC}/main.cpp)
target_link_libraries(tmapsvr_asio PRIVATE tmapsvr_asio_core)

# Tests — in-process, no DB. First suite: Automapper profile wiring
# (CharRow→CharSnapshot field-by-field + numeric narrowing).
add_executable(test_tmapsvr_asio_mapper_profiles
    ${TMAPSVR_ASIO_SRC}/tests/test_mapper_profiles.cpp)
target_link_libraries(test_tmapsvr_asio_mapper_profiles PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_mapper_profiles
    COMMAND test_tmapsvr_asio_mapper_profiles)

# world_senders byte-layout suite (MW_/RW_ encoders → TWorld).
add_executable(test_tmapsvr_asio_world_senders
    ${TMAPSVR_ASIO_SRC}/tests/test_world_senders.cpp)
target_link_libraries(test_tmapsvr_asio_world_senders PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_world_senders
    COMMAND test_tmapsvr_asio_world_senders)

# Inbound enter-server handler suite: OnMW_ENTERSVR_REQ resolves the
# char identity (cache hit / player-service load / missing) and replies
# MW_ENTERSVR_ACK. In-process against Fake* services, no DB, no sockets.
add_executable(test_tmapsvr_asio_world_enter
    ${TMAPSVR_ASIO_SRC}/tests/test_world_enter.cpp)
target_link_libraries(test_tmapsvr_asio_world_enter PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_world_enter
    COMMAND test_tmapsvr_asio_world_enter)

# Inbound per-connection entry handshake: OnMW_ENTERCHAR_REQ replies
# MW_ENTERCHAR_ACK (char_id, key) and tracks the composite spawn
# position. The link TWorld's CheckMainCon loop blocks on.
add_executable(test_tmapsvr_asio_world_enterchar
    ${TMAPSVR_ASIO_SRC}/tests/test_world_enterchar.cpp)
target_link_libraries(test_tmapsvr_asio_world_enterchar PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_world_enterchar
    COMMAND test_tmapsvr_asio_world_enterchar)

# client_senders byte-layout suite (CS_ acks the map relays to the
# client off inbound MW_ cluster traffic).
add_executable(test_tmapsvr_asio_client_senders
    ${TMAPSVR_ASIO_SRC}/tests/test_client_senders.cpp)
target_link_libraries(test_tmapsvr_asio_client_senders PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_client_senders
    COMMAND test_tmapsvr_asio_client_senders)

# Inbound main-cell check: OnMW_CHECKMAIN_REQ replies MW_CHECKMAIN_ACK
# only when the char is resident here (IsMainCell stand-in).
add_executable(test_tmapsvr_asio_world_checkmain
    ${TMAPSVR_ASIO_SRC}/tests/test_world_checkmain.cpp)
target_link_libraries(test_tmapsvr_asio_world_checkmain PRIVATE
    tmapsvr_asio_core)
add_test(NAME tmapsvr_asio_world_checkmain
    COMMAND test_tmapsvr_asio_world_checkmain)
