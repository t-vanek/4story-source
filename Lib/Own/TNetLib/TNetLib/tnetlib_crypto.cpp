// PCH-free (no stdafx.h) — compiles on Linux without TNetLib.h → winsock2.h.
// MSVC vcxproj marks this file with PrecompiledHeader=NotUsing.

#include "tnetlib_crypto.h"

#include <openssl/evp.h>
#include <openssl/opensslv.h>
#if OPENSSL_VERSION_MAJOR >= 3
#include <openssl/provider.h>
#endif

#include <cstring>

namespace tnetlib_crypto {

namespace {

constexpr int MD5_DIGEST_LEN = 16;

#if OPENSSL_VERSION_MAJOR >= 3
// OpenSSL 3 removed RC4 from the default provider — it lives in the
// "legacy" provider, which must be explicitly loaded once per process.
// MD5 is still in default; we load default explicitly anyway so that
// behavior is identical whether the host application has loaded a
// provider already or not. Keep our provider references until shutdown and
// release them before OpenSSL's automatic cleanup (registered during load).
// Unload releases only our references, not another component's provider handles.
void EnsureLegacyProviderLoaded() noexcept
{
    struct Providers {
        OSSL_PROVIDER* default_provider = OSSL_PROVIDER_load(nullptr, "default");
        OSSL_PROVIDER* legacy_provider = OSSL_PROVIDER_load(nullptr, "legacy");
        ~Providers() {
            if (legacy_provider) OSSL_PROVIDER_unload(legacy_provider);
            if (default_provider) OSSL_PROVIDER_unload(default_provider);
        }
    };
    // Function-local static initialization is thread-safe. A missing provider
    // remains null; EncryptInit reports RC4 unavailability to the caller.
    static const Providers providers;
    (void)providers;
}
#else
inline void EnsureLegacyProviderLoaded() noexcept {}
#endif

bool MD5HashSecret(const unsigned char* secret, std::size_t secret_len,
                   unsigned char digest[MD5_DIGEST_LEN]) noexcept
{
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (mdctx == nullptr) return false;

    bool ok = false;
    if (EVP_DigestInit_ex(mdctx, EVP_md5(), nullptr) == 1 &&
        EVP_DigestUpdate(mdctx, secret, secret_len) == 1)
    {
        unsigned int outlen = 0;
        if (EVP_DigestFinal_ex(mdctx, digest, &outlen) == 1 &&
            outlen == MD5_DIGEST_LEN)
        {
            ok = true;
        }
    }
    EVP_MD_CTX_free(mdctx);
    return ok;
}

// Apply RC4 in place using a precomputed key.
bool ApplyRC4(unsigned char* buf, std::size_t buf_len,
              const unsigned char* key, int key_len) noexcept
{
    EnsureLegacyProviderLoaded();
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) return false;

    bool ok = false;
    // EVP_rc4() is variable-key-length. We do a two-stage init: first to
    // bind the cipher, then to set the key length, then again to bind the
    // actual key bytes. This is the standard OpenSSL idiom for
    // variable-length ciphers (see EVP_EncryptInit_ex docs).
    if (EVP_EncryptInit_ex(ctx, EVP_rc4(), nullptr, nullptr, nullptr) == 1 &&
        EVP_CIPHER_CTX_set_key_length(ctx, key_len) == 1 &&
        EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, nullptr) == 1)
    {
        int outlen = 0;
        // RC4 is a stream cipher: outlen == buf_len, no padding, no final block.
        if (buf_len == 0)
        {
            ok = true;
        }
        else if (EVP_EncryptUpdate(ctx, buf, &outlen, buf, static_cast<int>(buf_len)) == 1 &&
                 outlen == static_cast<int>(buf_len))
        {
            ok = true;
        }
    }
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

} // namespace

bool RC4Transform(unsigned char* buf, std::size_t buf_len,
                  const unsigned char* key, std::size_t key_len) noexcept
{
    if (key_len == 0 || key_len > 256) return false; // RC4 spec: 1..256 byte key
    return ApplyRC4(buf, buf_len, key, static_cast<int>(key_len));
}

bool RC4MD5Transform(unsigned char* buf, std::size_t buf_len,
                     const unsigned char* secret, std::size_t secret_len) noexcept
{
    unsigned char rc4_key[MD5_DIGEST_LEN];
    if (!MD5HashSecret(secret, secret_len, rc4_key)) return false;

    const bool ok = ApplyRC4(buf, buf_len, rc4_key, MD5_DIGEST_LEN);
    // Best-effort wipe of the derived key on the way out.
    std::memset(rc4_key, 0, sizeof(rc4_key));
    return ok;
}

bool RC4MD5TransformCopy(unsigned char* out, std::size_t out_len,
                         const unsigned char* in, std::size_t in_len,
                         const unsigned char* secret, std::size_t secret_len) noexcept
{
    if (out_len < in_len) return false;
    if (out != in && in_len > 0)
    {
        std::memcpy(out, in, in_len);
    }
    return RC4MD5Transform(out, in_len, secret, secret_len);
}

} // namespace tnetlib_crypto
