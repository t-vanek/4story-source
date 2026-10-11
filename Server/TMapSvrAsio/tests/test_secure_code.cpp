#include "services/secure_code.h"
#include "services/client_senders.h"
#include <iostream>
using namespace tmapsvr;
int main(){unsigned n=0;
    const auto check=[&](bool ok){if(!ok)throw std::runtime_error("secure assertion "+std::to_string(n+1));++n;};
    try {
        SecureCodeState s;s.hydrated=true;constexpr auto now=1700000000000LL;
        const auto op=[&](std::uint16_t id,std::string code={},std::string replacement={}) {
            auto p=PlanSecureCode(s,{id,std::move(code),std::move(replacement)},now,0xffff0000U);s=p.state;return p.packet;};
        for(auto id:{0x5582,0x5584,0x5586,0x5588,0x5590,0x5592})check(!op(id,"NoCode00"));
        auto ack=op(0x5580,"CaseCode1");check(ack->first==0x5581&&ack->second==std::vector<std::byte>{std::byte{2}}&&s.unlocked&&!s.disabled&&!s.Blocks());
        ack=op(0x5580,"Different");check(ack->second[0]==std::byte{1}&&s.code=="CaseCode1");
        ack=op(0x5588,"CaseCode1");check(ack->first==0x5589&&ack->second==std::vector<std::byte>({std::byte{7},std::byte{1}})); // already unlocked is wrong
        s.unlocked=false;check(s.Blocks());
        ack=op(0x5590,"CaseCode1");check(ack->first==0x5591&&ack->second[0]==std::byte{9}&&!s.unlocked&&s.tries==0);
        ack=op(0x5582,"casecode1","NewCode2");check(ack->second[0]==std::byte{7}&&s.code=="CaseCode1");
        ack=op(0x5582,"CaseCode1","NewCode2");check(ack->second[0]==std::byte{5}&&s.code=="NewCode2"&&s.unlocked);
        for(auto id:{0x5582,0x5584,0x5588,0x5590}) {
            s.tries=9;s.unlocked=false;s.deadline_ms=0;s.source_tick=0;
            ack=op(id,"incorrect");
            check(s.tries==10&&s.deadline_ms==now+86400000&&s.source_tick==static_cast<std::uint32_t>(0xffff0000U+86400000U));
            check(ack->first==(id==0x5588?0x5591:id+1)&&ack->second[0]==std::byte(id==0x5588||id==0x5590?10:7)&&ack->second[1]==std::byte{10});
            ack=op(id,"NewCode2");check(ack->first==0x5591&&ack->second[0]==std::byte{10});
        }
        auto timed=PlanSecureCode(s,{0x5592},now+61000,0);check(timed.packet->first==0x5593&&timed.packet->second==std::vector<std::byte>({std::byte{23},std::byte{58}}));
        timed=PlanSecureCode(s,{0x5592},now+86400000,0);s=timed.state;
        check(timed.packet->first==0x5589&&timed.packet->second==std::vector<std::byte>({std::byte{0},std::byte{0}})&&!s.unlocked&&!s.deadline_ms&&!s.source_tick);
        ack=op(0x5588,"NewCode2");check(ack->second[0]==std::byte{8}&&s.unlocked);
        ack=op(0x5584,"NewCode2");check(ack->first==0x5585&&ack->second[0]==std::byte{6}&&s.disabled);
        s.unlocked=false;ack=op(0x5586);check(ack->second==std::vector<std::byte>{std::byte{4}}&&!s.disabled&&s.Blocks());
        CharSnapshot character;auto payload=std::make_shared<CharacterPayload>();payload->security=s;character.payload=payload;
        const auto info=EncodeCharInfoAck(character,"2000-01-01 00:00:00");check(info[4]==std::byte{1}&&info[5]==std::byte{0}&&info[6]==std::byte{0});
        for(auto id:{0x5580,0x5582,0x5584,0x5588,0x5590}) {
            std::vector<std::byte> body;wire::WriteString(body,"Eight123");if(id==0x5582)wire::WriteString(body,"Next4567");
            check(DecodeSecureRequest(id,body).code=="Eight123");body.push_back(std::byte{});
            bool rejected=false;try{DecodeSecureRequest(id,body);}catch(const std::runtime_error&){rejected=true;}check(rejected);
        }
        for(auto code:{std::string(56,'A'),std::string("ABC\0DEF",7)}) {
            std::vector<std::byte> b;wire::WriteString(b,code);bool rejected=false;
            try{DecodeSecureRequest(0x5580,b);}catch(...){rejected=true;}check(rejected);
        }
        std::cout<<"PASS "<<n<<" original secure-code assertions\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
