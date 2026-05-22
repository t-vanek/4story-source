#include "handlers.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>

namespace tworldsvr::handlers {

namespace {

// IK_* enum values from Lib/Own/TProtocol/include/NetCode.h
// (TITEM_KIND). The W3a-3 handler only branches on the identity-
// mutation cases; the remaining IK_* values (consumables / gear /
// SMS / etc.) are not legal payloads for CHANGECHARBASE — those
// flow through different DM_* handlers.
constexpr std::uint8_t kIkFace       = 45;
constexpr std::uint8_t kIkHair       = 46;
constexpr std::uint8_t kIkRace       = 47;
constexpr std::uint8_t kIkName       = 48;
constexpr std::uint8_t kIkSex        = 49;
constexpr std::uint8_t kIkCountry    = 96;
constexpr std::uint8_t kIkAidCountry = 97;

} // namespace

boost::asio::awaitable<void>
OnChangeCharBaseAck(std::shared_ptr<PeerSession>  peer,
                    std::vector<std::byte>        body,
                    const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();

    if (!ctx.chars)
    {
        spdlog::warn("OnChangeCharBaseAck[{}]: char registry not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0;
    std::uint32_t key     = 0;
    std::uint8_t  b_type  = 0;
    std::uint8_t  b_value = 0;
    std::uint16_t title   = 0;
    std::string   new_name;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(b_type) ||
        !r.Read(b_value) || !r.Read(title) || !r.ReadString(new_name))
    {
        spdlog::warn("OnChangeCharBaseAck[{}]: short body ({} bytes) — "
                     "dropped", ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar)
    {
        spdlog::warn("OnChangeCharBaseAck[{}]: char_id={} not in registry "
                     "— dropped", ip, char_id);
        co_return;
    }
    {
        std::lock_guard g(tchar->lock);
        if (tchar->key != key)
        {
            spdlog::warn("OnChangeCharBaseAck[{}]: char_id={} key mismatch "
                         "(registry=0x{:08X} incoming=0x{:08X}) — dropped",
                ip, char_id, tchar->key, key);
            co_return;
        }
    }

    // Per-bType mutation. NAME is special: it has cluster-wide
    // side-effects (legacy friend/soulmate notification, guild
    // app name updates) that aren't ported in W3a-3 — only the
    // name + name-index update lands here. The friend / soulmate
    // / guild-app notifications attach in W3a-4 / W4 once the
    // owning registries exist.
    switch (b_type)
    {
    case kIkFace:
    {
        std::lock_guard g(tchar->lock);
        tchar->face = b_value;
        spdlog::info("OnChangeCharBaseAck[{}]: char_id={} face={}", ip,
            char_id, b_value);
        break;
    }
    case kIkHair:
    {
        std::lock_guard g(tchar->lock);
        tchar->hair = b_value;
        spdlog::info("OnChangeCharBaseAck[{}]: char_id={} hair={}", ip,
            char_id, b_value);
        break;
    }
    case kIkRace:
    {
        std::lock_guard g(tchar->lock);
        tchar->race = b_value;
        spdlog::info("OnChangeCharBaseAck[{}]: char_id={} race={}", ip,
            char_id, b_value);
        break;
    }
    case kIkSex:
    {
        std::lock_guard g(tchar->lock);
        tchar->sex = b_value;
        spdlog::info("OnChangeCharBaseAck[{}]: char_id={} sex={}", ip,
            char_id, b_value);
        break;
    }
    case kIkCountry:
    {
        std::lock_guard g(tchar->lock);
        tchar->country = b_value;
        spdlog::info("OnChangeCharBaseAck[{}]: char_id={} country={} "
                     "(TODO W3a-4: ChangeCountry cluster fan-out)",
            ip, char_id, b_value);
        break;
    }
    case kIkAidCountry:
    {
        std::lock_guard g(tchar->lock);
        tchar->aid_country = b_value;
        spdlog::info("OnChangeCharBaseAck[{}]: char_id={} aid_country={}",
            ip, char_id, b_value);
        break;
    }
    case kIkName:
    {
        if (new_name.empty())
        {
            spdlog::warn("OnChangeCharBaseAck[{}]: char_id={} NAME branch "
                         "with empty new_name — dropped", ip, char_id);
            co_return;
        }
        if (!ctx.chars->Rename(char_id, new_name))
        {
            // Name collides with another char (cluster-wide
            // uniqueness violated). The legacy module silently
            // overwrites the name index entry; we refuse here so
            // FindByName never returns the wrong char on
            // collision. Operator can investigate.
            spdlog::warn("OnChangeCharBaseAck[{}]: char_id={} rename to "
                         "'{}' refused — name already taken", ip,
                char_id, new_name);
            co_return;
        }
        spdlog::info("OnChangeCharBaseAck[{}]: char_id={} renamed to '{}'",
            ip, char_id, new_name);
        // TODO W4 (friend): notify friends via SendDM_FRIENDERASE_REQ.
        // TODO W4 (soulmate): notify soulmate via SendDM_SOULMATEDEL_REQ.
        // TODO W3a-4 (guild apps): update FindGuildWantedApp /
        //   FindGuildTacticsWantedApp / pTCHAR->m_pGuild member /
        //   pTCHAR->m_pTactics member / FindTNMTPlayer.
        // TODO W3a-4: SendRW_CHANGENAME_ACK fan-out via PeerRegistry.
        break;
    }
    default:
        spdlog::info("OnChangeCharBaseAck[{}]: char_id={} unsupported "
                     "bType={} (W3a-3 handles 45/46/47/48/49/96/97 only)",
            ip, char_id, b_type);
        break;
    }
    co_return;
}

} // namespace tworldsvr::handlers
