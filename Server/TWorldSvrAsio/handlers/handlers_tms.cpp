#include "handlers.h"
#include "communication_authority.h"
#include "../senders/senders.h"
#include "../services/party_constants.h"
#include "../wire_codec.h"
#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

namespace tworldsvr::handlers {
namespace {
struct Route {
    std::shared_ptr<TChar> character;
    std::uint32_t id{},key{};
    std::uint8_t server{},klass{},level{},country{},aid{};
    std::string name;
    std::vector<std::uint32_t> rooms;
};
Route Snap(const HandlerContext& ctx,std::uint32_t id) {
    Route r;if(!ctx.chars)return r;r.character=ctx.chars->Find(id);if(!r.character)return r;
    std::lock_guard guard(r.character->lock);const auto& c=*r.character;
    r.id=id;r.key=c.key;r.server=c.main_server_id;r.klass=c.klass;r.level=c.level;
    r.country=c.country;r.aid=c.aid_country;r.name=c.name;r.rooms=c.tms;return r;
}
bool Ready(const HandlerContext& ctx,const Route& r) {
    return communication::Current(ctx,communication::Map(ctx,r.server),r.character,r.key);
}
bool HasRoom(const Route& r,std::uint32_t room) {
    return std::find(r.rooms.begin(),r.rooms.end(),room)!=r.rooms.end();
}
Route Member(const HandlerContext& ctx,const std::shared_ptr<TTms>& room,std::uint32_t id) {
    auto r=Snap(ctx,id);
    if(!Ready(ctx,r)||!HasRoom(r,room->id)){r.character.reset();return r;}
    std::lock_guard guard(room->lock);const auto key=room->member_keys.find(id);
    if(!room->IsMember(id)||key==room->member_keys.end()||key->second!=r.key)r.character.reset();return r;
}
void Add(const std::shared_ptr<TTms>& room,const Route& r) {
    {std::lock_guard guard(room->lock);room->AddMember(r.id);room->member_keys[r.id]=r.key;}
    {std::lock_guard guard(r.character->lock);
     if(std::find(r.character->tms.begin(),r.character->tms.end(),room->id)==r.character->tms.end())r.character->tms.push_back(room->id);}
}
void Remove(const std::shared_ptr<TTms>& room,const Route& r) {
    {std::lock_guard guard(room->lock);room->RemoveMember(r.id);room->member_keys.erase(r.id);room->pending.clear();room->last_member=r.name;}
    {std::lock_guard guard(r.character->lock);std::erase(r.character->tms,room->id);}
}
std::vector<std::uint32_t> Members(const std::shared_ptr<TTms>& room) {
    std::lock_guard guard(room->lock);auto ids=room->members;std::sort(ids.begin(),ids.end());return ids;
}
std::vector<senders::TmsMemberInfo> Roster(const HandlerContext& ctx,const std::shared_ptr<TTms>& room) {
    std::vector<senders::TmsMemberInfo> rows;
    for(auto id:Members(room))if(auto r=Member(ctx,room,id);r.character)rows.push_back({r.id,r.name,r.klass,r.level});
    return rows;
}
boost::asio::awaitable<void> Invite(const HandlerContext& ctx,const std::shared_ptr<TTms>& room,std::uint32_t inviter) {
    const auto roster=Roster(ctx,room);
    for(auto id:Members(room))if(auto r=Member(ctx,room,id);r.character)
        co_await senders::SendMwTmsInviteReq(communication::Map(ctx,r.server),r.id,r.key,inviter,room->id,roster);
}
boost::asio::awaitable<void> Receive(const HandlerContext& ctx,const std::shared_ptr<TTms>& room,
    const std::string& sender,const std::string& message) {
    for(auto id:Members(room))if(auto r=Member(ctx,room,id);r.character)
        co_await senders::SendMwTmsRecvReq(communication::Map(ctx,r.server),r.id,r.key,room->id,sender,message);
}
bool Text(const std::string& message){return message.find('\0')==std::string::npos;}
}

boost::asio::awaitable<void> OnTmsSendAck(std::shared_ptr<PeerSession> peer,std::vector<std::byte> body,const HandlerContext& ctx) {
    auto operation=co_await ctx.communication_operations->Acquire();
    if(!ctx.chars||!ctx.peers||!ctx.tms)co_return;
    wire::Reader reader(body.data(),body.size());std::uint32_t id=0,key=0,room_id=0;std::string message;
    if(!reader.Read(id)||!reader.Read(key)||!reader.Read(room_id)||!reader.ReadString(message)||!reader.Eof()||!Text(message))co_return;
    const auto sender=Snap(ctx,id);
    if(sender.key!=key||!(co_await communication::Authorize(ctx,peer,sender.character,key)).valid)co_return;
    const auto room=ctx.tms->Find(room_id);if(!room||!Member(ctx,room,id).character)co_return;
    if(Members(room).size()==1) {
        std::string last;{std::lock_guard guard(room->lock);last=room->last_member;}
        auto target=last.empty()?nullptr:ctx.chars->FindByName(last);
        if(target) {
            std::uint32_t target_id=0;{std::lock_guard guard(target->lock);target_id=target->char_id;}
            const auto recipient=Snap(ctx,target_id);
            if(!(co_await communication::Authorize(ctx,communication::Map(ctx,recipient.server),recipient.character,recipient.key)).valid||
               !Member(ctx,room,id).character)co_return;
            {std::lock_guard guard(room->lock);auto& pending=room->pending[target_id];
             std::erase_if(pending,[&](const auto& ask){return ask.target_key!=recipient.key;});
             if(pending.size()>=255)co_return;
             pending.push_back({id,key,recipient.key,message});}
            co_await senders::SendMwTmsInviteAskReq(communication::Map(ctx,recipient.server),recipient.id,recipient.key,id,key,room_id,message);
            co_return;
        }
        message=ServerNetString({},ServerMessage(ctx.server_messages,kTmsNoReceiverMessage));
    }
    co_await Receive(ctx,room,sender.name,message);
}

boost::asio::awaitable<void> OnTmsInviteAskAck(std::shared_ptr<PeerSession> peer,std::vector<std::byte> body,const HandlerContext& ctx) {
    auto operation=co_await ctx.communication_operations->Acquire();
    if(!ctx.chars||!ctx.peers||!ctx.tms)co_return;
    wire::Reader reader(body.data(),body.size());std::uint32_t id=0,key=0,target_id=0,target_key=0,room_id=0;
    std::uint8_t result=0;std::string message;
    if(!reader.Read(id)||!reader.Read(key)||!reader.Read(target_id)||!reader.Read(target_key)||!reader.Read(result)||
       !reader.Read(room_id)||!reader.ReadString(message)||!reader.Eof()||result>1||!Text(message))co_return;
    const auto room=ctx.tms->Find(room_id);if(!room)co_return;
    const auto sender=Member(ctx,room,id),target=Snap(ctx,target_id);
    if(!sender.character||sender.key!=key||target.key!=target_key)co_return;
    // The physical responding Map owns TARGET; the logical initiator is first
    // in the original ACK. Both generations must still authorize this prompt.
    if(!(co_await communication::Authorize(ctx,peer,target.character,target_key)).valid||
       !(co_await communication::Authorize(ctx,communication::Map(ctx,sender.server),sender.character,key)).valid||!Ready(ctx,target))co_return;
    {std::lock_guard guard(room->lock);auto pending=room->pending.find(target_id);if(pending==room->pending.end())co_return;
     const auto expected=std::find_if(pending->second.begin(),pending->second.end(),[&](const auto& ask){
         return ask.sender==id&&ask.sender_key==key&&ask.target_key==target_key&&ask.message==message;
     });
     if(expected==pending->second.end())co_return;
     pending->second.erase(expected);if(pending->second.empty())room->pending.erase(pending);
     if(result&&!room->IsMember(target_id)&&room->members.size()>=255)co_return;}
    if(result) {
        Add(room,target);
        // Source mixed the new target's ID with each recipient's key. Preserve
        // the original body shape, but address the actual recipient generation.
        co_await Invite(ctx,room,target_id);
    } else message=ServerNetString({},ServerMessage(ctx.server_messages,kTmsNoReceiverMessage));
    co_await Receive(ctx,room,sender.name,message);
}

boost::asio::awaitable<void> OnTmsInviteAck(std::shared_ptr<PeerSession> peer,std::vector<std::byte> body,const HandlerContext& ctx) {
    auto operation=co_await ctx.communication_operations->Acquire();
    if(!ctx.chars||!ctx.peers||!ctx.tms)co_return;
    wire::Reader reader(body.data(),body.size());std::uint32_t id=0,key=0,room_id=0;std::uint8_t count=0;
    if(!reader.Read(id)||!reader.Read(key)||!reader.Read(room_id)||!reader.Read(count)||!count)co_return;
    std::vector<std::uint32_t> ids;
    for(unsigned n=0;n<count;++n){std::uint32_t target=0;if(!reader.Read(target))co_return;ids.push_back(target);}
    if(!reader.Eof())co_return;
    const auto sender=Snap(ctx,id);
    if(sender.key!=key||!(co_await communication::Authorize(ctx,peer,sender.character,key)).valid)co_return;
    auto room=room_id?ctx.tms->Find(room_id):nullptr;
    if(room_id&&(!room||!Member(ctx,room,id).character))co_return;
    std::vector<Route> targets;std::sort(ids.begin(),ids.end());ids.erase(std::unique(ids.begin(),ids.end()),ids.end());
    for(auto target:ids) {
        const auto r=Snap(ctx,target);
        if(party::WarCountry(r.country,r.aid)!=party::WarCountry(sender.country,sender.aid))continue;
        if((co_await communication::Authorize(ctx,communication::Map(ctx,r.server),r.character,r.key)).valid)targets.push_back(r);
    }
    const auto current_sender=Snap(ctx,id);
    if(targets.empty()||current_sender.character!=sender.character||current_sender.key!=key||!Ready(ctx,current_sender))co_return;
    std::erase_if(targets,[&](auto& target){
        auto current=Snap(ctx,target.id);
        if(current.character!=target.character||current.key!=target.key||!Ready(ctx,current)||
           party::WarCountry(current.country,current.aid)!=party::WarCountry(current_sender.country,current_sender.aid))return true;
        target=std::move(current);return false;
    });if(targets.empty())co_return;
    if(!room&&targets.size()==1) {
        const auto& target=targets.front();
        for(auto candidate:target.rooms) {
            auto existing=ctx.tms->Find(candidate);if(!existing||!Member(ctx,existing,target.id).character)continue;
            bool match=false;{std::lock_guard guard(existing->lock);match=existing->members.size()==1&&existing->last_member==sender.name;}
            if(!match)continue;
            Add(existing,sender);
            // Source re-pair shortcut sends its roster only to the requester.
            const auto roster=Roster(ctx,existing);
            co_await senders::SendMwTmsInviteReq(peer,id,key,id,candidate,roster);co_return;
        }
    }
    if(!room) {
        const auto additional=std::count_if(targets.begin(),targets.end(),[&](const auto& r){return r.id!=id;});
        if(additional+1>255)co_return; // repair the original BYTE count overflow
        room=std::make_shared<TTms>();room->id=ctx.tms->GenId();if(!room->id||!ctx.tms->Insert(room))co_return;Add(room,sender);
    }
    {std::lock_guard guard(room->lock);std::size_t added=0;for(const auto& r:targets)if(!room->IsMember(r.id))++added;
     if(room->members.size()+added>255)co_return;}
    for(const auto& target:targets)Add(room,target);
    co_await Invite(ctx,room,id);
}

boost::asio::awaitable<void> OnTmsOutAck(std::shared_ptr<PeerSession> peer,std::vector<std::byte> body,const HandlerContext& ctx) {
    auto operation=co_await ctx.communication_operations->Acquire();
    if(!ctx.chars||!ctx.peers||!ctx.tms)co_return;
    wire::Reader reader(body.data(),body.size());std::uint32_t id=0,key=0,room_id=0;
    if(!reader.Read(id)||!reader.Read(key)||!reader.Read(room_id)||!reader.Eof())co_return;
    const auto room=ctx.tms->Find(room_id);if(!room)co_return;const auto sender=Member(ctx,room,id);
    if(!sender.character||sender.key!=key||!(co_await communication::Authorize(ctx,peer,sender.character,key)).valid)co_return;
    for(auto member:Members(room))if(auto recipient=Member(ctx,room,member);recipient.character)
        co_await senders::SendMwTmsOutReq(communication::Map(ctx,recipient.server),recipient.id,recipient.key,room_id,sender.name);
    Remove(room,sender);if(Members(room).empty())ctx.tms->Remove(room_id);
}

boost::asio::awaitable<void> NotifyTmsOnLogout(const HandlerContext& ctx,std::shared_ptr<TChar> who) {
    auto operation=co_await ctx.communication_operations->Acquire();
    if(!ctx.chars||!ctx.peers||!ctx.tms||!who)co_return;
    Route departing;departing.character=who;
    {std::lock_guard guard(who->lock);departing.id=who->char_id;departing.key=who->key;departing.name=who->name;departing.rooms=who->tms;}
    for(auto room_id:departing.rooms) {
        const auto room=ctx.tms->Find(room_id);if(!room)continue;
        {std::lock_guard guard(room->lock);const auto key=room->member_keys.find(departing.id);
         if(key==room->member_keys.end()||key->second!=departing.key)continue;}
        Remove(room,departing);
        for(auto member:Members(room))if(auto recipient=Member(ctx,room,member);recipient.character)
            co_await senders::SendMwTmsOutReq(communication::Map(ctx,recipient.server),recipient.id,recipient.key,room_id,departing.name);
        if(Members(room).empty())ctx.tms->Remove(room_id);
    }
}
}
