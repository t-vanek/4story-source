#pragma once
#include "communication_authority.h"

namespace tworldsvr::handlers::party_authority {
inline bool CanMutate(const HandlerContext& ctx,const std::shared_ptr<TChar>& character) {
    if(!character)return false;
    if(!ctx.chat_repo)return true;
    std::lock_guard g(character->lock);
    // An old real self/other DEL must be attempted before another membership
    // generation can join. Otherwise it could erase a later valid client JOIN.
    return character->deferred_party_removals.empty();
}
inline bool Current(const HandlerContext& ctx,const std::shared_ptr<PeerSession>& peer,
                    const std::shared_ptr<TChar>& character,std::uint32_t key) {
    if(!communication::Current(ctx,peer,character,key))return false;
    std::lock_guard guard(character->lock);
    return !character->main_handoff && character->chg_main_id==0;
}
inline boost::asio::awaitable<PartyIdentity> Authorize(const HandlerContext& ctx,
    const std::shared_ptr<PeerSession>& peer,const std::shared_ptr<TChar>& character) {
    if(!character)co_return PartyIdentity{};
    std::uint32_t key=0;std::string name;
    {std::lock_guard guard(character->lock);key=character->key;name=character->name;}
    if(!Current(ctx,peer,character,key))co_return PartyIdentity{};
    PartyIdentity result{true,false,name};
    // Native main always supplies this SQL authority. The absent repository is
    // the explicit legacy/test adapter, never an error fallback in production.
    if(ctx.chat_repo)result=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
        [repo=ctx.chat_repo,id=character->char_id,key,server=static_cast<std::uint8_t>(peer->Wid())]
        {return repo->AuthorizeParty(id,{key,server});});
    if(!Current(ctx,peer,character,key)||!result.valid||result.name!=name||
       (result.native&&result.map_token!=peer->NativeMapToken()))co_return PartyIdentity{};
    co_return result;
}
inline bool ModesCompatible(std::uint8_t origin,std::uint8_t target) {
    // Direct NetCode.h source values: BOW30, BR50. Normal Maps may differ.
    return !((origin==30||target==30||origin==50||target==50)&&origin!=target);
}
inline bool CurrentReplica(const HandlerContext& ctx,const std::shared_ptr<PeerSession>& peer,
    const std::shared_ptr<PeerSession>& main,const std::shared_ptr<TChar>& character,std::uint32_t key) {
    if(!Current(ctx,main,character,key)||!peer||!ctx.peers||!ctx.peers->IsCurrentMap(peer)||
       !peer->Wire()->IsOpen()||peer==main)return false;
    const auto server=static_cast<std::uint8_t>(peer->Wid());
    std::lock_guard g(character->lock);
    return character->key==key&&!character->closing&&!character->logout&&!character->main_handoff&&
        character->chg_main_id==0&&character->main_server_id==static_cast<std::uint8_t>(main->Wid())&&
        std::any_of(character->cons.begin(),character->cons.end(),[server](const auto& c){
            return c.server_id==server&&c.valid&&c.ready;});
}
inline boost::asio::awaitable<std::array<PartyIdentity,2>> AuthorizeReplica(const HandlerContext& ctx,
    const std::shared_ptr<PeerSession>& peer,const std::shared_ptr<PeerSession>& main,
    const std::shared_ptr<TChar>& character,std::uint32_t key) {
    if(!ctx.chat_repo||!CurrentReplica(ctx,peer,main,character,key))co_return std::array<PartyIdentity,2>{};
    std::string name;{std::lock_guard g(character->lock);name=character->name;}
    const auto result=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
        [repo=ctx.chat_repo,id=character->char_id,key,source=static_cast<std::uint8_t>(main->Wid()),
         target=static_cast<std::uint8_t>(peer->Wid())]
        {return repo->AuthorizePartyReplica(id,{key,source,key,target});});
    if(!CurrentReplica(ctx,peer,main,character,key)||!result[0].valid||!result[1].valid||
       result[0].name!=name||result[1].name!=name||result[0].map_token!=main->NativeMapToken()||
       result[1].map_token!=peer->NativeMapToken())co_return std::array<PartyIdentity,2>{};
    co_return result;
}
inline boost::asio::awaitable<std::array<PartyIdentity,2>> AuthorizePair(const HandlerContext& ctx,
    const std::shared_ptr<PeerSession>& first_peer,const std::shared_ptr<TChar>& first,
    const std::shared_ptr<PeerSession>& second_peer,const std::shared_ptr<TChar>& second) {
    if(!first||!second)co_return std::array<PartyIdentity,2>{};
    std::uint32_t first_key=0,second_key=0;std::string first_name,second_name;
    {std::lock_guard g(first->lock);first_key=first->key;first_name=first->name;}
    {std::lock_guard g(second->lock);second_key=second->key;second_name=second->name;}
    if(!Current(ctx,first_peer,first,first_key)||!Current(ctx,second_peer,second,second_key))
        co_return std::array<PartyIdentity,2>{};
    std::array<PartyIdentity,2> result{PartyIdentity{true,false,first_name},PartyIdentity{true,false,second_name}};
    if(ctx.chat_repo)result=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
        [repo=ctx.chat_repo,a=first->char_id,b=second->char_id,first_key,second_key,
         first_server=static_cast<std::uint8_t>(first_peer->Wid()),second_server=static_cast<std::uint8_t>(second_peer->Wid())]
        {return repo->AuthorizePartyPair(a,b,{first_key,first_server,second_key,second_server});});
    if(!Current(ctx,first_peer,first,first_key)||!Current(ctx,second_peer,second,second_key)||
       !result[0].valid||!result[1].valid||result[0].name!=first_name||result[1].name!=second_name||
       (result[0].native&&result[0].map_token!=first_peer->NativeMapToken())||
       (result[1].native&&result[1].map_token!=second_peer->NativeMapToken()))
        co_return std::array<PartyIdentity,2>{};
    co_return result;
}
}
