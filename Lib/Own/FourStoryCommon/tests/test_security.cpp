// Smoke test for fourstory::security primitives:
//   * IpAllowlist (CIDR parse + match)
//   * Hmac (sign + verify + hex round-trip)
//   * PeerAuthToken (encode/decode + HMAC bind to source addr)
//   * NonceCache (replay rejection)
//   * PeerSecurityGate (in-memory mode, no DB)

#include "fourstory/security/ip_allowlist.h"
#include "fourstory/security/hmac.h"
#include "fourstory/security/peer_auth_token.h"
#include "fourstory/security/peer_security_gate.h"
#include "fourstory/security/security_config.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

int main()
{
    using namespace fourstory::security;

    // ── IpAllowlist ──────────────────────────────────────────────────
    {
        IpAllowlist al(false);
        assert(al.Add("10.0.0.0/8"));
        assert(al.Add("127.0.0.1"));
        assert(al.Add("192.168.1.0/24"));
        assert(al.Allows("10.5.5.5"));
        assert(al.Allows("127.0.0.1"));
        assert(al.Allows("192.168.1.42"));
        assert(!al.Allows("192.168.2.1"));
        assert(!al.Allows("8.8.8.8"));
        assert(!al.Add("malformed"));
        assert(!al.Add("999.0.0.0"));
    }

    // ── Hmac round-trip + constant-time verify ──────────────────────
    {
        const std::string key  = "test-key-bytes-here";
        const std::string msg  = "hello world";
        const auto bytes = std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(key.data()), key.size());
        auto d1 = Hmac::Sign(bytes, msg);
        auto d2 = Hmac::Sign(bytes, msg);
        assert(Hmac::Verify(d1, d2));
        auto d3 = Hmac::Sign(bytes, "different msg");
        assert(!Hmac::Verify(d1, d3));
        const auto hex = Hmac::ToHex(d1);
        Hmac::Digest parsed{};
        assert(Hmac::FromHex(hex, parsed));
        assert(Hmac::Verify(d1, parsed));
        assert(!Hmac::FromHex("too-short", parsed));
    }

    // ── PeerAuthToken Build → Encode → Decode round-trip ───────────
    {
        const auto secret = Hmac::HexToBytes(
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
        auto tok = PeerAuthToken::Build(secret,
            /*peer_type*/ 4, /*group*/ 1, /*server*/ 2,
            "10.0.0.5", 1700000000ULL, 0xCAFEBABEDEADBEEFULL);

        auto wire = tok.Encode();
        assert(wire.size() == PeerAuthToken::kWireSize);
        PeerAuthToken parsed{};
        assert(PeerAuthToken::Decode(wire, parsed));
        assert(parsed.timestamp == tok.timestamp);
        assert(parsed.nonce     == tok.nonce);
        assert(parsed.peer_type == tok.peer_type);
        assert(Hmac::Verify(parsed.hmac, tok.hmac));

        // Verify HMAC binds the source address — recomputing with a
        // different addr must NOT match.
        const auto good = Hmac::Sign(secret,
            PeerAuthToken::ComposeMessage(tok.timestamp, tok.nonce,
                tok.peer_type, tok.group_id, tok.server_id, "10.0.0.5"));
        const auto bad  = Hmac::Sign(secret,
            PeerAuthToken::ComposeMessage(tok.timestamp, tok.nonce,
                tok.peer_type, tok.group_id, tok.server_id, "10.0.0.99"));
        assert(Hmac::Verify(good, tok.hmac));
        assert(!Hmac::Verify(bad,  tok.hmac));
    }

    // ── NonceCache replay rejection ─────────────────────────────────
    {
        NonceCache nc(std::chrono::seconds(60));
        assert(nc.TryRecord(4, 1700000000, 1, 1700000000));
        assert(!nc.TryRecord(4, 1700000000, 1, 1700000005)); // replay
        assert(nc.TryRecord(4, 1700000000, 2, 1700000005));  // different nonce
        assert(nc.TryRecord(5, 1700000000, 1, 1700000005));  // different type
    }

    // ── PeerSecurityGate (in-memory mode, no DB) ────────────────────
    {
        SecurityConfig cfg;
        cfg.ip_allowlist          = {"10.0.0.0/8", "127.0.0.1"};
        cfg.ip_allowlist_enforce  = true;
        cfg.peer_auth_required    = false;   // no DB trust store in this test
        cfg.master_key_hex        = "abcd1234";
        assert(cfg.Validate().empty());

        PeerSecurityGate gate(cfg, nullptr);

        // IP filter
        assert(gate.CheckIp("10.0.0.5").allowed());
        assert(gate.CheckIp("127.0.0.1").allowed());
        const auto denied = gate.CheckIp("8.8.8.8");
        assert(!denied.allowed());
        assert(denied.outcome == PeerAuthOutcome::IpDenied);

        // Token check is a no-op when peer_auth_required=false
        PeerAuthToken tok{};
        const auto ok = gate.CheckToken(tok, "10.0.0.5", 1700000000);
        assert(ok.allowed());
    }

    // ── SecurityConfig validation ───────────────────────────────────
    {
        SecurityConfig cfg;
        cfg.peer_auth_required = true;
        cfg.master_key_env = "FOURSTORY_TEST_KEY_THAT_DOES_NOT_EXIST_xyz";
        cfg.master_key_hex = "";
        assert(!cfg.Validate().empty());  // must complain about missing key
        cfg.ip_allowlist = {"10.0.0.0/33"};
        const auto err = cfg.Validate();
        assert(err.find("ip_allowlist") != std::string::npos);

        // future_window must accept 0 (mandates exact clock sync) but
        // not negative.
        SecurityConfig fcfg;
        fcfg.master_key_hex = "deadbeef";
        fcfg.future_window = std::chrono::seconds(0);
        assert(fcfg.Validate().empty());
        fcfg.future_window = std::chrono::seconds(-1);
        const auto fwerr = fcfg.Validate();
        assert(fwerr.find("future_window") != std::string::npos);
    }

    // ── Asymmetric timestamp window ─────────────────────────────────
    // Verifies CheckToken's freshness check rejects future-skewed
    // tokens with the tight `future_window` bound and past-skewed
    // tokens with the wider `nonce_window` bound. peer_auth_required
    // is on, so we exercise the freshness branch; trust map is empty
    // (no DB), so accepted tokens fall through to UnknownPeer — that's
    // the signal that freshness passed.
    {
        SecurityConfig cfg;
        cfg.peer_auth_required = true;
        cfg.master_key_hex     = "00112233445566778899aabbccddeeff";
        cfg.nonce_window       = std::chrono::seconds(30);
        cfg.future_window      = std::chrono::seconds(3);
        cfg.audit_failed_attempts = false;  // no DB → would crash on log
        assert(cfg.Validate().empty());

        PeerSecurityGate gate(cfg, nullptr);
        const std::uint64_t now = 1'700'000'000ULL;

        auto mk = [](std::uint64_t ts) {
            PeerAuthToken t{};
            t.timestamp = ts;
            t.nonce     = 1;
            t.peer_type = 4;
            t.group_id  = 1;
            t.server_id = 2;
            return t;
        };

        // Future +2s — inside future_window → freshness OK → UnknownPeer.
        {
            const auto r = gate.CheckToken(mk(now + 2), "10.0.0.5", now);
            assert(r.outcome == PeerAuthOutcome::UnknownPeer);
        }
        // Future +10s — outside future_window → Expired.
        {
            const auto r = gate.CheckToken(mk(now + 10), "10.0.0.5", now);
            assert(r.outcome == PeerAuthOutcome::Expired);
            assert(r.reason.find("future_window") != std::string::npos ||
                   r.reason.find("+3s") != std::string::npos);
        }
        // Past -20s — inside nonce_window → freshness OK → UnknownPeer.
        {
            const auto r = gate.CheckToken(mk(now - 20), "10.0.0.5", now);
            assert(r.outcome == PeerAuthOutcome::UnknownPeer);
        }
        // Past -100s — outside nonce_window → Expired.
        {
            const auto r = gate.CheckToken(mk(now - 100), "10.0.0.5", now);
            assert(r.outcome == PeerAuthOutcome::Expired);
        }
        // Exact `now` — trivially fresh.
        {
            const auto r = gate.CheckToken(mk(now), "10.0.0.5", now);
            assert(r.outcome == PeerAuthOutcome::UnknownPeer);
        }
    }

    std::printf("test_security: all assertions passed\n");
    return 0;
}
