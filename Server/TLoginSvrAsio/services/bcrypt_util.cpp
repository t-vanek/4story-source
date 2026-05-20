// BCrypt helpers — IsBcrypt prefix test + MakeBcryptHash wrapper.
//
// Hash shape and work factor are centralized here so the online auth
// path (SociAuthService::Authenticate / VerifyPassword) and the
// offline tloginsvr_bcrypt_migrate tool produce + accept exactly the
// same hash bytes.

#include "bcrypt_util.h"

#include <bcrypt/bcrypt.h>

namespace tloginsvr::services::bcrypt_util {

// Returns true iff `s` looks like a libbcrypt hash (starts with one of
// the recognized `$2a$ / $2b$ / $2y$` prefixes). Used by the auth
// path to decide between bcrypt verify and reject-as-legacy.
bool IsBcrypt(const std::string& s)
{
    return s.size() >= 4
        && s[0] == '$' && s[1] == '2'
        && (s[2] == 'a' || s[2] == 'b' || s[2] == 'y')
        && s[3] == '$';
}

// Hash `value` with the requested bcrypt work factor. Returns the
// 60-byte hash string on success, empty string on libbcrypt failure
// (callers treat empty as "abort the migration / abort the rehash"
// — never persist an empty hash into TACCOUNT_PW.szPasswd).
std::string MakeBcryptHash(const std::string& value, int cost)
{
    char salt[BCRYPT_HASHSIZE]{};
    if (::bcrypt_gensalt(cost, salt) != 0) return {};
    char hash[BCRYPT_HASHSIZE]{};
    if (::bcrypt_hashpw(value.c_str(), salt, hash) != 0) return {};
    return std::string(hash);
}

} // namespace tloginsvr::services::bcrypt_util
