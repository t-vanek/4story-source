#pragma once
#include "fourstory/db/session_pool.h"
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>

namespace tworldsvr {
using ServerMessages=std::map<std::uint32_t,std::string>;
// CTProtocol.h server-message IDs. The content comes only from the publication.
inline constexpr std::uint32_t kTmsNoReceiverMessage=5;
inline constexpr std::uint32_t kOperatorNameMessage=6;
inline std::string ServerMessage(const ServerMessages* messages,std::uint32_t id) {
    if(messages)if(auto it=messages->find(id);it!=messages->end())return it->second;
    return {}; // Original GetSvrMsg returns NAME_NULL for an absent key.
}
inline std::string ServerNetString(const std::string& header,const std::string& body) {
    if(header.size()>65535||body.size()>65535)throw std::runtime_error("Source net string exceeds WORD length");
    constexpr char hex[]="0123456789ABCDEF";std::string lengths(8,'0');
    for(unsigned n=0;n<4;++n) {
        lengths[3-n]=hex[(header.size()>>(n*4))&15];
        lengths[7-n]=hex[(body.size()>>(n*4))&15];
    }
    return lengths+header+body; // Original MultiByte BuildNetString.
}
ServerMessages LoadPostgreSQLServerMessages(fourstory::db::SessionPool&,std::uint8_t world,
    const std::string& owner_token,const std::string& client_encoding);
}
