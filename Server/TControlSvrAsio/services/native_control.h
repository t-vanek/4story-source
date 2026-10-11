#pragma once

#include "fourstory/db/session_pool.h"
#include <memory>
#include <atomic>
#include <stdexcept>
#include <string>
#include <string_view>

namespace tcontrolsvr::native_control {
inline bool Enabled(const fourstory::db::SessionPool& pool) {
    return pool.GetBackend()==fourstory::db::Backend::PostgreSQL;
}
inline std::string Hex(std::string_view bytes) {
    constexpr char hex[]="0123456789abcdef";std::string result;result.reserve(bytes.size()*2);
    for(const unsigned char b:bytes){result+=hex[b>>4];result+=hex[b&15];}
    return result;
}
inline std::string Bytes(std::string_view hex) {
    const auto digit=[](char c)->unsigned {
        if(c>='0'&&c<='9')return c-'0';
        if(c>='a'&&c<='f')return c-'a'+10;
        throw std::runtime_error("Invalid stored Control bytes");
    };
    if(hex.size()%2)throw std::runtime_error("Invalid stored Control bytes");
    std::string out;out.reserve(hex.size()/2);
    for(std::size_t i=0;i<hex.size();i+=2)out+=static_cast<char>((digit(hex[i])<<4)|digit(hex[i+1]));
    return out;
}

// One session owns the cluster Control lock for its lifetime. Mutating
// transactions fence the exact generation and backend start, so loss of that
// session cannot leave a stale process writing after another Control starts.
class Owner {
public:
    explicit Owner(fourstory::db::SessionPool& pool);
    ~Owner();
    Owner(const Owner&)=delete;
    Owner& operator=(const Owner&)=delete;
    std::unique_ptr<soci::transaction> Begin(soci::session& sql) const;
    void Check(soci::session& sql) const;
    void Invalidate() { m_uncertain=true; }
private:
    fourstory::db::SessionPool::Lease m_lease;
    long long m_generation=0;
    int m_pid=0;
    std::atomic_bool m_uncertain{false};
};
inline std::unique_ptr<soci::transaction> Begin(soci::session& sql,const std::shared_ptr<Owner>& owner) {
    if(!owner)throw std::runtime_error("Native Control writer is not configured");
    return owner->Begin(sql);
}
inline void Commit(soci::transaction& tx,std::atomic_bool& uncertain,const std::shared_ptr<Owner>& owner) {
    try{tx.commit();}catch(...){uncertain=true;if(owner)owner->Invalidate();throw;}
}
} // namespace tcontrolsvr::native_control
