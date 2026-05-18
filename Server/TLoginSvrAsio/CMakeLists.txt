# TLoginSvrAsio — modernized login server, runs alongside the legacy
# TLoginSvr.exe. Phase 3 of step E in the modernization plan; see
# README.md in this directory.

set(TLOGINSVR_ASIO_SRC ${CMAKE_CURRENT_SOURCE_DIR})

# Shared library of the server's logic, so the integration test can
# instantiate LoginServer in-process instead of forking the binary.
add_library(tloginsvr_asio_core STATIC
    ${TLOGINSVR_ASIO_SRC}/login_server.cpp
    ${TLOGINSVR_ASIO_SRC}/handlers.cpp)
target_include_directories(tloginsvr_asio_core PUBLIC
    ${TLOGINSVR_ASIO_SRC}
    ${CMAKE_SOURCE_DIR}/Lib/Own/TNetLib/TNetLib
    ${FOURSTORY_TPROTOCOL_INCLUDE_DIR})
target_link_libraries(tloginsvr_asio_core PUBLIC tnetlib_portable)
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
