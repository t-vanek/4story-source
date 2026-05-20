# fourstory_common — shared infrastructure for the modernized servers
# (TLoginSvrAsio, TPatchSvrAsio, TLogSvrAsio). Pulled out so each server
# doesn't carry its own copy of the SOCI session pool, audit/SMTP
# plumbing, admin shell, health endpoint, rate limiter, and registry
# refresher.
#
# Public include layout: consumers write `#include "fourstory/db/...h"`,
# the directory tree under fourstory/ mirrors the namespace tree.

set(FOURSTORY_COMMON_HEADERS
    fourstory/db/session_pool.h
    fourstory/db/schema_validator.h
    fourstory/db/co_offload.h
    fourstory/audit/audit_logger.h
    fourstory/audit/spdlog_audit_logger.h
    fourstory/audit/udp_audit_logger.h
    fourstory/smtp/smtp_client.h
    fourstory/smtp/spdlog_smtp_client.h
    fourstory/smtp/asio_smtp_client.h
    fourstory/ops/admin_shell.h
    fourstory/ops/health_endpoint.h
    fourstory/ops/rate_limiter.h
    fourstory/ops/registry_refresher.h
)

set(FOURSTORY_COMMON_SOURCES
    src/db/session_pool.cpp
    src/db/schema_validator.cpp
    src/audit/spdlog_audit_logger.cpp
    src/audit/udp_audit_logger.cpp
    src/smtp/spdlog_smtp_client.cpp
    src/smtp/asio_smtp_client.cpp
    src/ops/admin_shell.cpp
    src/ops/health_endpoint.cpp
    src/ops/rate_limiter.cpp
    src/ops/registry_refresher.cpp
)

add_library(fourstory_common STATIC
    ${FOURSTORY_COMMON_HEADERS}
    ${FOURSTORY_COMMON_SOURCES}
)

target_include_directories(fourstory_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Boost.Asio (headers only — system supplies error codes), spdlog, SOCI.
find_package(Boost CONFIG QUIET COMPONENTS system)
if(NOT Boost_FOUND)
    # Debian/Ubuntu: header-only Asio usage + libboost_system from
    # libboost-system-dev. No CMake config file shipped → plain link.
    find_package(Boost REQUIRED COMPONENTS system)
endif()
find_package(spdlog QUIET)
find_package(SOCI CONFIG QUIET)

target_link_libraries(fourstory_common
    PUBLIC
        Boost::system
)

if(spdlog_FOUND)
    target_link_libraries(fourstory_common PUBLIC spdlog::spdlog)
else()
    # Distro libspdlog-dev: header path is /usr/include/spdlog,
    # plain link name is `spdlog`.
    target_link_libraries(fourstory_common PUBLIC spdlog)
endif()

if(SOCI_FOUND)
    if(TARGET SOCI::soci_core)
        target_link_libraries(fourstory_common PUBLIC SOCI::soci_core)
    elseif(TARGET SOCI::soci_core_static)
        target_link_libraries(fourstory_common PUBLIC SOCI::soci_core_static)
    endif()
    # ODBC backend is used at runtime via SOCI's dynamic backend loader;
    # we keep the include path consistent with the SOCI ODBC backend so the
    # session_pool.cpp `#include <soci/odbc/soci-odbc.h>` resolves.
    if(TARGET SOCI::soci_odbc)
        target_link_libraries(fourstory_common PUBLIC SOCI::soci_odbc)
    endif()
else()
    # Debian/Ubuntu libsoci-dev. The dynamic backend loader pulls the
    # backend .sos in lazily — listing soci_odbc / soci_sqlite3 here
    # just makes them link-resolvable when the SOCI dlopen path looks
    # them up. dl is required on Linux for the dynamic loader.
    target_link_libraries(fourstory_common PUBLIC
        soci_sqlite3
        soci_odbc
        soci_core
        dl)
endif()
