#pragma once
#include "domain/character.h"
#include "wire_codec.h"
#include <optional>
#include <span>
#include <stdexcept>

namespace tmapsvr {
using SecurePacket=std::pair<std::uint16_t,std::vector<std::byte>>;
struct SecureRequest {std::uint16_t opcode{};std::string code,replacement;};
struct SecurePlan {SecureCodeState state;std::optional<SecurePacket> packet;};
struct SecureCommit {std::shared_ptr<const CharSnapshot> snapshot;std::optional<SecurePacket> packet;};
inline SecureRequest DecodeSecureRequest(std::uint16_t opcode,std::span<const std::byte> body) {
    SecureRequest q;q.opcode=opcode;wire::Reader r(body.data(),body.size());
    switch(opcode) {
    case 0x5580:case 0x5584:case 0x5588:case 0x5590:
        if(!r.ReadString(q.code))throw std::runtime_error("Invalid secure request");
        break;
    case 0x5582:
        if(!r.ReadString(q.code)||!r.ReadString(q.replacement))throw std::runtime_error("Invalid secure request");
        break;
    case 0x5586:case 0x5592:break;
    default:throw std::runtime_error("Unknown secure request");
    }
    // Backup column is VARCHAR(55). Never truncate or reinterpret text bytes.
    // The original client submits 8..12 bytes; reject NUL rather than allowing
    // CString/SQL truncation to create a different credential after reconnect.
    if(!r.Eof()||q.code.size()>55||q.replacement.size()>55||q.code.find('\0')!=std::string::npos||q.replacement.find('\0')!=std::string::npos)
        throw std::runtime_error("Invalid secure request");
    return q;
}
inline SecurePacket SecureUnlockPrompt(const SecureCodeState& s,std::uint8_t result=0) {
    // CSSender.cpp redirects UNLOCK to CHECK at the ten-attempt threshold.
    return {static_cast<std::uint16_t>(s.tries>=10?0x5591:0x5589),{std::byte(s.tries>=10?10:result),std::byte(s.tries)}};
}
inline SecurePlan PlanSecureCode(const SecureCodeState& before,const SecureRequest& q,
    std::int64_t now_ms,std::uint32_t source_now) {
    SecurePlan out{before,{}};auto& s=out.state;
    const auto ack=[&](std::uint8_t code,bool attempts=true){
        out.packet=SecurePacket{static_cast<std::uint16_t>(q.opcode+1),{std::byte(code)}};
        if(attempts)out.packet->second.push_back(std::byte(s.tries));
    };
    if(q.opcode==0x5580) {
        if(s.created){ack(1,false);return out;}
        s.created=true;s.disabled=false;s.unlocked=true;s.code=q.code;s.tries=0;s.source_tick=0;s.deadline_ms=0;
        ack(2,false);return out;
    }
    if(!s.created)return out;
    if(q.opcode==0x5586){s.disabled=false;ack(4,false);return out;}
    if(q.opcode==0x5592) {
        if(!s.deadline_ms||s.tries<10)return out;
        if(now_ms>=s.deadline_ms){s.tries=0;s.source_tick=0;s.deadline_ms=0;out.packet=SecureUnlockPrompt(s);}
        else {const auto seconds=(s.deadline_ms-now_ms)/1000;
            out.packet=SecurePacket{0x5593,{std::byte((seconds/3600)%24),std::byte((seconds/60)%60)}};}
        return out;
    }
    if(s.tries>=10){out.packet=SecureUnlockPrompt(s);return out;}
    if(q.code==s.code&&(q.opcode!=0x5588||!s.unlocked)) {
        s.tries=0;
        switch(q.opcode) {
        case 0x5582:s.code=q.replacement;s.source_tick=0;s.deadline_ms=0;s.unlocked=true;ack(5);break;
        case 0x5584:s.disabled=true;s.unlocked=true;ack(6);break;
        case 0x5588:s.unlocked=true;out.packet=SecureUnlockPrompt(s,8);break;
        case 0x5590:ack(9);break;
        }
    }else {
        ++s.tries;
        if(s.tries>=10&&!s.deadline_ms){s.deadline_ms=now_ms+86400000;s.source_tick=source_now+86400000U;}
        if(q.opcode==0x5588)out.packet=SecureUnlockPrompt(s,7);
        else ack(q.opcode==0x5590&&s.tries>=10?10:7);
    }
    return out;
}
}
