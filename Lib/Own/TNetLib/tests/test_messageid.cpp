// Sanity checks for the generated MessageId enum + NameOf table.
// No PCH, no winsock; pure header + one .cpp.

#include "MessageId.h"

#include <cstdio>
#include <string_view>

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

void TestKnownValues()
{
    using tnetlib::protocol::MessageId;
    std::printf("[known value pinpoints]\n");
    // The handful of IDs that are referenced from a dozen different
    // places in the codebase. Pinning their numeric values here catches
    // any future drift in the generator vs the legacy macros.
    Check(static_cast<std::uint16_t>(MessageId::CS_LOGIN_REQ) == 0x1988,
          "CS_LOGIN_REQ == 0x1988 (CS_LOGIN + 0x0001)");
    Check(static_cast<std::uint16_t>(MessageId::CS_LOGIN_ACK) == 0x1989,
          "CS_LOGIN_ACK == 0x1989");
    Check(static_cast<std::uint16_t>(MessageId::CS_CONNECT_REQ) == 0x5281,
          "CS_CONNECT_REQ == 0x5281 (CS_MAP + 0x0001)");
    Check(static_cast<std::uint16_t>(MessageId::MW_CONNECT_ACK) == 0x9002,
          "MW_CONNECT_ACK == 0x9002 (MW_BASE + 0x0001)");
    Check(static_cast<std::uint16_t>(MessageId::DM_ENTERMAPSVR_REQ) == 0x5892,
          "DM_ENTERMAPSVR_REQ == 0x5892 (DM_BASE + 0x0001)");
    Check(static_cast<std::uint16_t>(MessageId::CT_EVENTMSG_REQ) == 0x9366,
          "CT_EVENTMSG_REQ == 0x9366");
}

void TestRoundTrip()
{
    using namespace tnetlib::protocol;
    std::printf("[ToMessageId / ToUint16 round-trip]\n");
    constexpr std::uint16_t raw = 0x1988;
    constexpr auto id = ToMessageId(raw);
    Check(ToUint16(id) == raw, "uint16 → MessageId → uint16 preserves value");
    Check(id == MessageId::CS_LOGIN_REQ, "round-trip lands on the right enumerator");
}

void TestNameOf()
{
    using namespace tnetlib::protocol;
    std::printf("[NameOf diagnostic lookup]\n");
    Check(NameOf(MessageId::CS_LOGIN_REQ) == "CS_LOGIN_REQ", "CS_LOGIN_REQ name");
    Check(NameOf(MessageId::MW_CONNECT_ACK) == "MW_CONNECT_ACK", "MW_CONNECT_ACK name");
    Check(NameOf(static_cast<MessageId>(0xDEAD)).empty(),
          "unknown ID returns empty view");
}

} // namespace

int main()
{
    std::printf("=== tnetlib MessageId tests ===\n");
    TestKnownValues();
    TestRoundTrip();
    TestNameOf();
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
