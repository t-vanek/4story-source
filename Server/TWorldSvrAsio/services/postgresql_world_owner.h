#pragma once
#include "fourstory/db/session_pool.h"
#include <cstdint>
#include <memory>
#include <string>
namespace tworldsvr {
class PostgreSQLWorldOwner {
public:
    PostgreSQLWorldOwner(const std::string& connection,std::uint8_t world);
    const std::string& Token() const {return m_token;}
    bool Healthy();
private:
    fourstory::db::SessionPool m_control;
    fourstory::db::SessionPool::Lease m_lease;
    std::string m_token;
    int m_world{},m_pid{};
};
std::unique_ptr<soci::transaction> BeginWorldTransaction(soci::session&,int world,const std::string& token,
                                                       bool snapshot=false);
}
