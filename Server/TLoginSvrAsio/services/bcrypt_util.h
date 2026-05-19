#pragma once

// Shared BCrypt helpers used by SociAuthService (login + delchar gate)
// and by the offline tloginsvr_bcrypt_migrate tool. Centralized so the
// hash shape and work factor stay in lockstep between online auth and
// offline batch migration.

#include <string>

namespace tloginsvr::services::bcrypt_util {

// Returns true when `s` looks like a BCrypt hash (`$2a$`/`$2b$`/`$2y$`
// prefix). Any other shape — including legacy SHA1-hex or raw
// plaintext — is rejected by the server's auth path after the
// migration cutover.
bool IsBcrypt(const std::string& s);

// Computes a fresh BCrypt hash at work factor 10. Returns the 60-char
// `$2a$10$<22-salt><31-hash>` string on success, an empty string when
// libbcrypt fails (essentially only when the system PRNG can't seed —
// the caller treats this as a fatal/skip condition).
//
// Cost 10 matches the work factor we picked for the transparent
// upgrade path; admins can re-rehash offline at a higher cost via
// the migration tool's --cost flag if needed.
std::string MakeBcryptHash(const std::string& value, int cost = 10);

} // namespace tloginsvr::services::bcrypt_util
