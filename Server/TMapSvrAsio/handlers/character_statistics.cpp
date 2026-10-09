#include "handlers.h"
#include "services/char_state_store.h"
#include "services/client_senders.h"
#include "services/session_registry.h"
#include "services/world_client.h"
#include "wire_codec.h"
#include "MessageId.h"
#include <spdlog/spdlog.h>

namespace tmapsvr {
namespace {
std::shared_ptr<tnetlib::AsioSession> ReadyPrimary(const HandlerContext& ctx,std::uint32_t id) {
    auto client=ctx.session_reg?ctx.session_reg->Find(id):nullptr;
    const auto identity=client?ctx.session_reg->Identity(client.get()):std::nullopt;
    return client&&client->IsOpen()&&identity&&identity->phase==SessionPhase::Ready&&
        identity->role==MapSessionRole::Primary?client:nullptr;
}
std::vector<std::byte> Statistics(const HandlerContext& ctx,std::uint32_t id) {
    if(!ctx.char_state)return {};
    const auto state=ctx.char_state->Get(id);
    if(!state||state->persistence_uncertain||!state->payload||!state->payload->statistics)return {};
    const auto& values=*state->payload->statistics;
    if(values.level!=state->bLevel||values.aftermath!=state->bAftermath||state->cluster.guild)return {};
    return EncodeCharacterStatistics(*state,values);
}
}

boost::asio::awaitable<void> OnCharStatInfoReq(std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte> body,const HandlerContext& ctx) {
    if(body.size()!=4)throw std::runtime_error("Invalid CHARSTATINFO request length");
    const auto identity=ctx.session_reg?ctx.session_reg->Identity(sess.get()):std::nullopt;
    if(!identity||ReadyPrimary(ctx,identity->char_id)!=sess)co_return;
    wire::Reader r(body.data(),body.size());std::uint32_t target{};r.Read(target);
    if(ReadyPrimary(ctx,target)) {
        auto answer=Statistics(ctx,target);
        if(answer.empty()){spdlog::debug("CHARSTATINFO target={} has unsupported derived state",target);co_return;}
        co_await sess->SendPacket(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_CHARSTATINFO_ACK),answer);
    }else if(ctx.world_client&&ctx.world_client->IsRegistered()) {
        std::vector<std::byte> request;wire::WritePOD(request,identity->char_id);wire::WritePOD(request,target);
        co_await ctx.world_client->SendPacket(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::MW_CHARSTATINFO_ACK),std::move(request));
    }
}

boost::asio::awaitable<void> OnWorldCharStatInfoAnsReq(std::span<const std::byte> body,const HandlerContext& ctx) {
    if(body.size()!=8||!ctx.world_client||!ctx.world_client->IsRegistered())co_return;
    wire::Reader r(body.data(),body.size());std::uint32_t requester{},target{};r.Read(requester);r.Read(target);
    if(!ReadyPrimary(ctx,target))co_return;
    auto stats=Statistics(ctx,target);if(stats.empty())co_return;
    std::vector<std::byte> reply;wire::WritePOD(reply,requester);reply.insert(reply.end(),stats.begin(),stats.end());
    co_await ctx.world_client->SendPacket(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::MW_CHARSTATINFOANS_ACK),std::move(reply));
}

boost::asio::awaitable<void> OnWorldCharStatInfoReq(std::span<const std::byte> body,const HandlerContext& ctx) {
    if(body.size()!=91||!ctx.world_client||!ctx.world_client->IsRegistered())co_return;
    wire::Reader r(body.data(),body.size());std::uint32_t requester{};r.Read(requester);
    auto client=ReadyPrimary(ctx,requester);if(!client)co_return;
    std::vector<std::byte> reply(body.begin()+4,body.end());
    co_await client->SendPacket(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_CHARSTATINFO_ACK),reply);
}
}
