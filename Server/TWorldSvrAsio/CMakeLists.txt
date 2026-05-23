# TWorldSvrAsio — modernized 4Story cluster coordinator.
#
# W1 ships the scaffold: TOML config, accept loop, packet framing,
# dispatch stub (logs + drops every wID). W2 adds the char SOCI
# repository + first batch of CHAR / USER / OnRW handlers. See
# Server/TWorldSvrAsio/README.md §4 for the full W2..W7 rollout.

set(TWORLD_ASIO_SRC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(spdlog QUIET)
find_package(tomlplusplus QUIET)
find_package(SOCI CONFIG QUIET)

# SOCI link list — needed transitively through fourstory_common even
# though W1 doesn't open any DB session itself.
set(TWORLD_SOCI_LIBS "")
if(SOCI_FOUND)
    if(TARGET SOCI::soci_core)
        list(APPEND TWORLD_SOCI_LIBS SOCI::soci_core)
    elseif(TARGET SOCI::soci_core_static)
        list(APPEND TWORLD_SOCI_LIBS SOCI::soci_core_static)
    endif()
    if(TARGET SOCI::soci_odbc)
        list(APPEND TWORLD_SOCI_LIBS SOCI::soci_odbc)
    elseif(TARGET SOCI::soci_odbc_static)
        list(APPEND TWORLD_SOCI_LIBS SOCI::soci_odbc_static)
    endif()
else()
    list(APPEND TWORLD_SOCI_LIBS soci_odbc soci_core dl)
endif()

# Core library — shared between the binary and the test executable
# so tests link against the exact same TUs without rebuilding.
add_library(tworldsvr_asio_core STATIC
    ${TWORLD_ASIO_SRC}/config.cpp
    ${TWORLD_ASIO_SRC}/world_session.cpp
    ${TWORLD_ASIO_SRC}/world_server.cpp
    ${TWORLD_ASIO_SRC}/db/schema_validator.cpp
    ${TWORLD_ASIO_SRC}/handlers/dispatch.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_char.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_char_base.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_guild.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_relay.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_relay.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_guild.cpp
    ${TWORLD_ASIO_SRC}/services/char_registry.cpp
    ${TWORLD_ASIO_SRC}/services/guild_broadcast.cpp
    ${TWORLD_ASIO_SRC}/services/guild_level_cache.cpp
    ${TWORLD_ASIO_SRC}/services/guild_peerage.cpp
    ${TWORLD_ASIO_SRC}/services/guild_registry.cpp
    ${TWORLD_ASIO_SRC}/services/guild_wanted_registry.cpp
    ${TWORLD_ASIO_SRC}/services/guild_wanted_sweep.cpp
    ${TWORLD_ASIO_SRC}/services/guild_tactics_wanted_registry.cpp
    ${TWORLD_ASIO_SRC}/services/guild_tactics_sweep.cpp
    ${TWORLD_ASIO_SRC}/services/guild_cabinet_codec.cpp
    ${TWORLD_ASIO_SRC}/services/pvp_aggregate.cpp
    ${TWORLD_ASIO_SRC}/services/fake_guild_level_repository.cpp
    ${TWORLD_ASIO_SRC}/services/fake_guild_repository.cpp
    ${TWORLD_ASIO_SRC}/services/soci_guild_level_repository.cpp
    ${TWORLD_ASIO_SRC}/services/soci_guild_repository.cpp
    ${TWORLD_ASIO_SRC}/services/peer_registry.cpp)

target_include_directories(tworldsvr_asio_core PUBLIC
    ${TWORLD_ASIO_SRC})

target_link_libraries(tworldsvr_asio_core PUBLIC
    fourstory_common
    tnetlib_portable
    ${TWORLD_SOCI_LIBS})

if(spdlog_FOUND)
    target_link_libraries(tworldsvr_asio_core PUBLIC spdlog::spdlog)
else()
    target_link_libraries(tworldsvr_asio_core PUBLIC spdlog)
endif()
if(tomlplusplus_FOUND)
    target_link_libraries(tworldsvr_asio_core PUBLIC tomlplusplus::tomlplusplus)
endif()

set_target_properties(tworldsvr_asio_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

add_executable(tworldsvr_asio ${TWORLD_ASIO_SRC}/main.cpp)
target_link_libraries(tworldsvr_asio PRIVATE tworldsvr_asio_core)

# ----- Tests --------------------------------------------------------
#
# W1 wire test: stands the WorldServer up on an ephemeral port,
# opens a loopback socket, frames a packet with a known + an unknown
# wID, and verifies (a) both are dispatched without closing the
# connection, (b) a checksum-mismatch frame DOES close the connection.
add_executable(test_tworldsvr_asio_dispatch
    ${TWORLD_ASIO_SRC}/tests/test_dispatch.cpp)
target_link_libraries(test_tworldsvr_asio_dispatch PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_dispatch
    COMMAND test_tworldsvr_asio_dispatch)

# W2 unit test: CharRegistry partitioning + per-char actor model.
# Exercises Insert / Find / Remove + concurrent inserts across
# threads + the active-user index. Pure logic — no asio, no DB.
add_executable(test_tworldsvr_asio_char_registry
    ${TWORLD_ASIO_SRC}/tests/test_char_registry.cpp)
target_link_libraries(test_tworldsvr_asio_char_registry PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_char_registry
    COMMAND test_tworldsvr_asio_char_registry)

# W2 wire test: drives MW_ADDCHAR_ACK + MW_CLOSECHAR_ACK frames
# through the dispatcher over a loopback session, asserts the
# CharRegistry state after each. Verifies the binding from
# wire → handler → registry mutation is correct.
add_executable(test_tworldsvr_asio_char_handlers
    ${TWORLD_ASIO_SRC}/tests/test_char_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_char_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_char_handlers
    COMMAND test_tworldsvr_asio_char_handlers)

# W3a-1 unit test: GuildRegistry partition / lifecycle. Pure
# logic, no asio. Mirrors test_char_registry's coverage.
add_executable(test_tworldsvr_asio_guild_registry
    ${TWORLD_ASIO_SRC}/tests/test_guild_registry.cpp)
target_link_libraries(test_tworldsvr_asio_guild_registry PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_guild_registry
    COMMAND test_tworldsvr_asio_guild_registry)

# W3a-1 wire test: drives DM_GUILDLOAD_ACK across a loopback
# session and checks the GuildRegistry insert + idempotent
# rejection of duplicate guild_id + char-not-in-registry drop.
add_executable(test_tworldsvr_asio_guild_handlers
    ${TWORLD_ASIO_SRC}/tests/test_guild_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_guild_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_guild_handlers
    COMMAND test_tworldsvr_asio_guild_handlers)

# W3a-1 fake-repo behavior: AddGuild seeds, LoadAll round-trips,
# FindById returns nullopt for misses. Pure logic.
add_executable(test_tworldsvr_asio_fake_guild_repo
    ${TWORLD_ASIO_SRC}/tests/test_fake_guild_repository.cpp)
target_link_libraries(test_tworldsvr_asio_fake_guild_repo PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_fake_guild_repo
    COMMAND test_tworldsvr_asio_fake_guild_repo)

# W3a-2 unit test: PeerRegistry lifecycle (Register / Find /
# Unregister, duplicate rejection, Snapshot). Pure logic.
add_executable(test_tworldsvr_asio_peer_registry
    ${TWORLD_ASIO_SRC}/tests/test_peer_registry.cpp)
target_link_libraries(test_tworldsvr_asio_peer_registry PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_peer_registry
    COMMAND test_tworldsvr_asio_peer_registry)

# W3a-2 wire test: drives RW_RELAYSVR_REQ over loopback, asserts
# (a) PeerRegistry gains an entry under the sent wID, (b) the
# RW_RELAYSVR_ACK reply carries the configured nation byte and the
# empty operator + msg lists. Verifies the full Wire → PeerSession
# → Registry path that W3a-3 handlers depend on.
add_executable(test_tworldsvr_asio_relay_handlers
    ${TWORLD_ASIO_SRC}/tests/test_relay_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_relay_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_relay_handlers
    COMMAND test_tworldsvr_asio_relay_handlers)

# W3a-3 unit test: CharRegistry secondary name index, including
# case-insensitive lookup, Rename, collision rejection, Remove
# dropping both indices, and concurrent renames on disjoint chars.
add_executable(test_tworldsvr_asio_char_name_index
    ${TWORLD_ASIO_SRC}/tests/test_char_name_index.cpp)
target_link_libraries(test_tworldsvr_asio_char_name_index PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_char_name_index
    COMMAND test_tworldsvr_asio_char_name_index)

# W3a-3 wire test: drives MW_CHANGECHARBASE_ACK over loopback,
# asserts the per-bType mutation lands on TChar (face/hair/race/sex/
# country/aid_country/name) + the name-index update on IK_NAME +
# the collision-refused branch.
add_executable(test_tworldsvr_asio_char_base_handlers
    ${TWORLD_ASIO_SRC}/tests/test_char_base_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_char_base_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_char_base_handlers
    COMMAND test_tworldsvr_asio_char_base_handlers)

# W3a-3 wire test: OnRW_ENTERCHAR_REQ (hit + miss) and
# OnRW_RELAYCONNECT_REQ routing to the right map peer by
# main_server_id, plus the cluster-wide RELAYCONNECT broadcast
# triggered by a second RELAYSVR registration.
add_executable(test_tworldsvr_asio_rw_lookup_handlers
    ${TWORLD_ASIO_SRC}/tests/test_rw_lookup_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_rw_lookup_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_rw_lookup_handlers
    COMMAND test_tworldsvr_asio_rw_lookup_handlers)

# W3a-4 wire test: MW_GUILDLEAVE_ACK + TChar.guild_id back-pointer
# invariant + the side-effect on a subsequent OnEnterCharReq.
add_executable(test_tworldsvr_asio_guild_mut_handlers
    ${TWORLD_ASIO_SRC}/tests/test_guild_mut_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_guild_mut_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_guild_mut_handlers
    COMMAND test_tworldsvr_asio_guild_mut_handlers)

# W3a-4d unit test: GuildLevelCache + FakeGuildLevelRepository
# round-trip + out-of-range row filtering + LoadFrom replace.
add_executable(test_tworldsvr_asio_guild_level_cache
    ${TWORLD_ASIO_SRC}/tests/test_guild_level_cache.cpp)
target_link_libraries(test_tworldsvr_asio_guild_level_cache PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_guild_level_cache
    COMMAND test_tworldsvr_asio_guild_level_cache)

# W3a-5 unit test: CheckPeerage gate. 7 scenarios cover every
# branch of the legacy switch + the W3a-4d "null level_row =
# relaxed dev gate" path.
add_executable(test_tworldsvr_asio_guild_peerage
    ${TWORLD_ASIO_SRC}/tests/test_guild_peerage.cpp)
target_link_libraries(test_tworldsvr_asio_guild_peerage PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_guild_peerage
    COMMAND test_tworldsvr_asio_guild_peerage)

# W3a-19 unit test: GuildWantedRegistry::PruneExpired +
# SweepExpiredWanted coroutine. Verifies the periodic prune
# only drops entries past end_time, cleans the applicant
# reverse index, and persists the deletions via the repo.
add_executable(test_tworldsvr_asio_guild_wanted_sweep
    ${TWORLD_ASIO_SRC}/tests/test_guild_wanted_sweep.cpp)
target_link_libraries(test_tworldsvr_asio_guild_wanted_sweep PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_guild_wanted_sweep
    COMMAND test_tworldsvr_asio_guild_wanted_sweep)

# W3a-28 unit test: CalcWeekRecord per-day vRecord trim + sum.
# Pure-logic, 6 scenarios covering empty / fresh / mixed / stale /
# boundary / idempotency.
add_executable(test_tworldsvr_asio_pvp_aggregate
    ${TWORLD_ASIO_SRC}/tests/test_pvp_aggregate.cpp)
target_link_libraries(test_tworldsvr_asio_pvp_aggregate PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_pvp_aggregate
    COMMAND test_tworldsvr_asio_pvp_aggregate)

# W3a-36 unit test: SweepExpiredTactics — prunes expired tactics
# contracts across the guild registry + clears char back-pointers.
add_executable(test_tworldsvr_asio_tactics_sweep
    ${TWORLD_ASIO_SRC}/tests/test_tactics_sweep.cpp)
target_link_libraries(test_tworldsvr_asio_tactics_sweep PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_tactics_sweep
    COMMAND test_tworldsvr_asio_tactics_sweep)
