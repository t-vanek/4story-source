#pragma once

// Cross-platform crypto primitives for TNetLib. Backed by OpenSSL EVP.
//
// Wire-format note: the algorithms here are chosen to be byte-for-byte
// compatible with the legacy Win32 CryptoAPI calls in CryptographyExt.cpp
// (CryptAcquireContext + CryptCreateHash(MD5) + CryptDeriveKey(RC4) +
// CryptEncrypt). Any modernization that breaks wire compatibility would
// require updating every legacy client too.
//
// The two functions exposed here are intentionally minimal and PCH-free
// (no stdafx.h, no winsock, no ATL) so they compile in any toolchain that
// has a C++20 compiler and OpenSSL headers.

#include <cstddef>

namespace tnetlib_crypto {

// Raw RC4 stream cipher, in-place. Caller supplies the RC4 key directly
// (no derivation). Useful for known-answer testing against RFC 6229
// vectors. RC4 is symmetric: the same call encrypts and decrypts.
// Returns true on success.
bool RC4Transform(unsigned char* buf, std::size_t buf_len,
                  const unsigned char* key, std::size_t key_len) noexcept;

// Derives an RC4 key from `secret` via MD5(secret) (using the first 16
// bytes of the digest as a 128-bit RC4 key) and applies RC4 in place.
// Equivalent to the Win32 chain
//     CryptCreateHash(CALG_MD5) → CryptHashData(secret)
//     CryptDeriveKey(CALG_RC4, hMD5, CRYPT_EXPORTABLE)
//     CryptEncrypt(...)
// on Windows XP-SP3 and later, which derive a 128-bit RC4 key from the
// full MD5 digest. (Pre-XP-SP3 systems used a 40-bit export-grade key;
// that case is not supported here.)
//
// Returns true on success. RC4 is symmetric — same call decrypts.
bool RC4MD5Transform(unsigned char* buf, std::size_t buf_len,
                     const unsigned char* secret, std::size_t secret_len) noexcept;

// Convenience variant: copies `in` → `out` first, then transforms `out`
// in place. Used by callers that historically passed distinct in/out
// buffers (CryptographyExt.cpp's EncryptBuffer signature).
bool RC4MD5TransformCopy(unsigned char* out, std::size_t out_len,
                         const unsigned char* in, std::size_t in_len,
                         const unsigned char* secret, std::size_t secret_len) noexcept;

} // namespace tnetlib_crypto
