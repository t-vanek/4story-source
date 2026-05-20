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
    ${TCONTROL_ASIO_SRC}/senders.cpp
    ${TCONTROL_ASIO_SRC}/handlers/handlers_auth.cpp
    ${TCONTROL_ASIO_SRC}/services/fake_operator_auth_service.cpp
    ${TCONTROL_ASIO_SRC}/services/operator_registry.cpp)

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
