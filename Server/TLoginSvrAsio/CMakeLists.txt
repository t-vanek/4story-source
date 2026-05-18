# TLoginSvrAsio — modernized login server, runs alongside the legacy
# TLoginSvr.exe. Phase 3 of step E in the modernization plan; see
# README.md in this directory.

set(TLOGINSVR_ASIO_SRC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(spdlog QUIET)

# Shared library of the server's logic, so the integration test can
# instantiate LoginServer in-process instead of forking the binary.
add_library(tloginsvr_asio_core STATIC
    ${TLOGINSVR_ASIO_SRC}/login_server.cpp
    ${TLOGINSVR_ASIO_SRC}/handlers.cpp
    ${TLOGINSVR_ASIO_SRC}/services/in_memory_auth_service.cpp)
target_include_directories(tloginsvr_asio_core PUBLIC
    ${TLOGINSVR_ASIO_SRC}
    ${CMAKE_SOURCE_DIR}/Lib/Own/TNetLib/TNetLib
    ${FOURSTORY_TPROTOCOL_INCLUDE_DIR})
target_link_libraries(tloginsvr_asio_core PUBLIC tnetlib_portable)
if(spdlog_FOUND)
    target_link_libraries(tloginsvr_asio_core PUBLIC spdlog::spdlog)
else()
    # Distro-installed without CMake config — fall back to the plain
    # library + header path. Works on Debian/Ubuntu where the package
    # is libspdlog-dev.
    target_link_libraries(tloginsvr_asio_core PUBLIC spdlog)
endif()
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
