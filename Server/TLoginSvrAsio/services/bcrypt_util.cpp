#include "bcrypt_util.h"

#include <bcrypt/bcrypt.h>

namespace tloginsvr::services::bcrypt_util {

bool IsBcrypt(const std::string& s)
{
    return s.size() >= 4
        && s[0] == '$' && s[1] == '2'
        && (s[2] == 'a' || s[2] == 'b' || s[2] == 'y')
        && s[3] == '$';
}

std::string MakeBcryptHash(const std::string& value, int cost)
{
    char salt[BCRYPT_HASHSIZE]{};
    if (::bcrypt_gensalt(cost, salt) != 0) return {};
    char hash[BCRYPT_HASHSIZE]{};
    if (::bcrypt_hashpw(value.c_str(), salt, hash) != 0) return {};
    return std::string(hash);
}

} // namespace tloginsvr::services::bcrypt_util
