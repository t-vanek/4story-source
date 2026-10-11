#include "soci_operator_auth_service.h"

#include "fourstory/db/orm/sp_call.h"

#include <spdlog/spdlog.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <array>

namespace tcontrolsvr {

SociOperatorAuthService::SociOperatorAuthService(fourstory::db::SessionPool& pool, std::shared_ptr<native_control::Owner> owner)
    : m_owner(std::move(owner)), m_pool(pool)
{
}

OperatorAuthResult
SociOperatorAuthService::Authenticate(const std::string& user_id,
                                      const std::string& password)
{
    using fourstory::db::orm::SpCall;

    OperatorAuthResult res{};
    try
    {
        if(native_control::Enabled(m_pool)) {
            if(user_id.size()>50||password.size()>50||user_id.find('\0')!=std::string::npos||
                password.find('\0')!=std::string::npos)return res;
            auto lease=m_pool.Acquire();auto tx=native_control::Begin(*lease,m_owner);
            const auto name=native_control::Hex(user_id);
            std::string salt_hex,hash_hex;int iterations=0,authority=0;
            (*lease) << "SELECT encode(password_salt,'hex'),encode(password_hash,'hex'),password_iterations,authority "
                        "FROM app_control.operators WHERE rtrim(name)=rtrim(convert_from(decode(:name,'hex'),'WIN1252'))",
                soci::use(name),soci::into(salt_hex),soci::into(hash_hex),soci::into(iterations),soci::into(authority);
            if(!lease->got_data())return res;
            const auto salt=native_control::Bytes(salt_hex),hash=native_control::Bytes(hash_hex);
            if(salt.size()!=16||hash.size()!=32||iterations<210000||iterations>2000000||authority<1||authority>255)
                throw std::runtime_error("Invalid native operator credential");
            auto length=password.find_last_not_of(' ');length=length==std::string::npos?0:length+1;
            std::array<unsigned char,32> derived{};
            if(PKCS5_PBKDF2_HMAC(password.data(),static_cast<int>(length),
                reinterpret_cast<const unsigned char*>(salt.data()),static_cast<int>(salt.size()),iterations,
                EVP_sha256(),static_cast<int>(derived.size()),derived.data())!=1)
                throw std::runtime_error("Operator credential derivation failed");
            const bool matched=CRYPTO_memcmp(derived.data(),hash.data(),derived.size())==0;
            OPENSSL_cleanse(derived.data(),derived.size());tx->commit();
            if(matched){res.ok=true;res.authority=static_cast<std::uint8_t>(authority);}
            return res;
        }
        // Legacy contract: TOPLogin's EXEC return code is the operator's
        // bAuthority value (0 = reject, 1 = MANAGER_ALL, 2 = SERVICE, …).
        // We capture it via WithReturn() — no OUT parameters needed.
        auto lease = m_pool.Acquire();
        auto r = SpCall("TOPLogin")
            .In("szUserID", user_id)
            .In("szPasswd", password)
            .WithReturn()
            .Execute(*lease);

        if (!r.Ok())
            return res;

        const auto authority = r.ReturnCode();
        if (authority < 0 || authority > 255)
        {
            spdlog::warn("soci_operator_auth: TOPLogin returned out-of-range "
                         "authority={} for id='{}'", authority, user_id);
            return res;
        }
        if (authority == 0)
            return res;  // legacy contract: 0 = reject

        res.ok        = true;
        res.authority = static_cast<std::uint8_t>(authority);
    }
    catch (const std::exception& ex)
    {
        if(native_control::Enabled(m_pool))throw std::runtime_error("Native Control authentication unavailable");
        spdlog::error("soci_operator_auth: TOPLogin('{}') DB error: {}",
            user_id, ex.what());
    }
    return res;
}

} // namespace tcontrolsvr
