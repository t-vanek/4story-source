# Vendored libbcrypt (trusch/libbcrypt) — Solar Designer's crypt_blowfish
# + Ricardo Garcia's C wrapper. Public domain. Used by TLoginSvrAsio's
# SociAuthService to verify TACCOUNT_PW.szPasswd hashes.
#
# Why vendored: no vcpkg port for libbcrypt and libxcrypt's Windows
# support is fragile. Source is ~2k lines total, all public domain.

add_library(fourstory_bcrypt STATIC
    src/bcrypt.c
    src/crypt_blowfish.c
    src/crypt_gensalt.c
    src/wrapper.c)

target_include_directories(fourstory_bcrypt PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
# Windows: NOT adding include/bcrypt/ to PRIVATE search path —
# Windows 11 24H2 SDK's wincrypt.h / ncrypt.h reference CNG types
# from <bcrypt.h>, and we want that to resolve to the Windows SDK
# header. The libbcrypt sources include their own bcrypt.h via
# explicit relative path ("../include/bcrypt/bcrypt.h") to avoid
# the collision.
#
# Non-Windows: no collision exists; the C sources use bare
# `#include "crypt_gensalt.h"` etc. which fail without the
# include/bcrypt path on the PRIVATE search path. The relative
# path `../include/bcrypt/bcrypt.h` they also include works either
# way.
if(NOT WIN32)
    target_include_directories(fourstory_bcrypt PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/bcrypt)
endif()

# crypt_blowfish.c uses #warning when no portable __asm__ is available;
# MSVC doesn't have that and treats it as an error under /W3. Silence the
# matching warning so the C file compiles cleanly on MSVC.
if(MSVC)
    target_compile_options(fourstory_bcrypt PRIVATE /wd4068 /wd4244 /wd4267 /wd4146 /wd4334)
    target_compile_definitions(fourstory_bcrypt PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(fourstory_bcrypt PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    FOLDER "3rdParty")
