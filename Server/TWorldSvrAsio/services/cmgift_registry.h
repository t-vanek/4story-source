#pragma once

// CmGiftRegistry — the cash-gift catalogue (legacy m_mapCMGift,
// TCMGIFT rows loaded from TCMGIFTCHART at boot and mutated by the
// operator CHARTUPDATE flow). Consumed by the W6-45 gift-delivery
// pipeline (CT_CMGIFT_REQ / MW_CMGIFT_ACK) and the operator list.

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace tworldsvr {

// CMGIFT_RESULT (NetCode.h:875).
inline constexpr std::uint8_t kCmGiftSuccess   = 0;
inline constexpr std::uint8_t kCmGiftTarget    = 1;
inline constexpr std::uint8_t kCmGiftId        = 2;
inline constexpr std::uint8_t kCmGiftDuplicate = 3;
inline constexpr std::uint8_t kCmGiftErrPost   = 4;
inline constexpr std::uint8_t kCmGiftFail      = 5;

// CMGIFTUPDATE_TYPE (NetCode.h:2640).
inline constexpr std::uint8_t kCguNone   = 0;
inline constexpr std::uint8_t kCguDel    = 1;
inline constexpr std::uint8_t kCguAdd    = 2;
inline constexpr std::uint8_t kCguUpdate = 3;

// One TCMGIFT row (TWorldType.h:1359).
struct CmGift
{
    std::uint16_t gift_id        = 0;
    std::uint8_t  gift_type      = 0;
    std::uint32_t value          = 0;
    std::uint8_t  count          = 0;
    std::uint8_t  take_type      = 0;  // 1 → per-target CanTake check
    std::uint8_t  max_take_count = 0;
    std::uint8_t  tool_only      = 0;  // gate: tool_only <= bTool
    std::uint16_t err_gift_id    = 0;  // DUPLICATE fallback gift
    std::string   title;
    std::string   msg;
};

class CmGiftRegistry
{
public:
    CmGiftRegistry() = default;
    CmGiftRegistry(const CmGiftRegistry&) = delete;
    CmGiftRegistry& operator=(const CmGiftRegistry&) = delete;

    // Boot hydrate from TCMGIFTCHART.
    void LoadFrom(const std::vector<CmGift>& rows);

    std::optional<CmGift> Get(std::uint16_t gift_id) const;

    // CGU_ADD semantics: map::insert — no-op when the id exists.
    void Add(CmGift gift);
    // CGU_UPDATE semantics: replace only when the id exists.
    void Update(CmGift gift);
    bool Erase(std::uint16_t gift_id);

    // Ordered snapshot for CT_CMGIFTLIST_ACK.
    std::vector<CmGift> Snapshot() const;

    std::size_t Size() const;

private:
    mutable std::mutex               m_mtx;
    std::map<std::uint16_t, CmGift>  m_gifts;
};

} // namespace tworldsvr
