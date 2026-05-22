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
    ${TWORLD_ASIO_SRC}/handlers/handlers_guild.cpp
    ${TWORLD_ASIO_SRC}/services/char_registry.cpp
    ${TWORLD_ASIO_SRC}/services/guild_registry.cpp
    ${TWORLD_ASIO_SRC}/services/fake_guild_repository.cpp
    ${TWORLD_ASIO_SRC}/services/soci_guild_repository.cpp)

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
