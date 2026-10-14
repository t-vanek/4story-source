#pragma once
#include "handlers.h"
#include "fourstory/db/co_offload.h"
#include <algorithm>
#include <mutex>
namespace tworldsvr::handlers::communication {
inline std::shared_ptr<PeerSession> Map(const HandlerContext& ctx,std::uint8_t server) {
    return server&&ctx.peers?ctx.peers->Find(static_cast<std::uint16_t>(0x0400U|server)):nullptr;
}
inline bool Current(const HandlerContext& ctx,const std::shared_ptr<PeerSession>& peer,
                    const std::shared_ptr<TChar>& character,std::uint32_t key) {
    if(!character||!peer||!ctx.chars||ctx.chars->Find(character->char_id)!=character||!ctx.peers||
       ctx.peers->Find(peer->Wid())!=peer||!peer->Wire()->IsOpen())return false;
    const auto server=static_cast<std::uint8_t>(peer->Wid());std::lock_guard guard(character->lock);
    const auto connection=std::find_if(character->cons.begin(),character->cons.end(),
        [server](const auto& row){return row.server_id==server;});
    return peer->Wid()==(0x0400U|server)&&character->key==key&&!character->closing&&!character->logout&&
        !character->db_loading&&character->main_server_id==server&&connection!=character->cons.end()&&
        connection->ready&&connection->valid;
}
inline boost::asio::awaitable<ChatAuthority> Authorize(const HandlerContext& ctx,
    const std::shared_ptr<PeerSession>& peer,const std::shared_ptr<TChar>& character,std::uint32_t key) {
    if(!Current(ctx,peer,character,key))co_return ChatAuthority{};
    ChatAuthority result{true,false,0};
    // Explicit legacy/test cache adapter. Native main always supplies chat_repo
    // and uses its verified SQL authority; no production fallback on SQL error.
    if(!ctx.chat_repo) {
        if(ctx.operators)result.is_operator=std::find(ctx.operators->begin(),ctx.operators->end(),character->char_id)!=ctx.operators->end();
        std::lock_guard guard(character->lock);result.ban_until=character->chat_ban_time;
    }
    if(ctx.chat_repo)result=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
        [repo=ctx.chat_repo,id=character->char_id,key,server=static_cast<std::uint8_t>(peer->Wid())]
        {return repo->Authorize(id,{key,server});});
    if(!Current(ctx,peer,character,key))co_return ChatAuthority{};
    co_return result;
}
}
