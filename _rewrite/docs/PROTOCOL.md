# 4Story 5.0 Wire Protocol — Reverse-Engineering Notes

Status: **Phase 0 complete** (2026-05-17). This document is the **contract** that `FourStory.Protocol` (C#) must implement byte-for-byte so the original C++ client connects to the new server.

## Phase 0 — what's done, what's deferred

| Task | Status | Coverage |
|------|--------|----------|
| §1 Frame format (16-byte header, primitives) | ✅ Done | Complete |
| §2 Encryption (XOR + RC4 + header obfuscation) | ✅ Done | Complete; 5 open Qs in §5 to resolve via packet capture |
| §3 Session state machine | ✅ Done | High-level; activation point of `m_bUseCrypt` TBD |
| §4 Ports & process map | ✅ Done | Complete |
| §4b Packet ID catalog (1542 IDs) | ✅ Done | Full table in [`packet-ids.csv`](packet-ids.csv); 9 collisions flagged |
| §4c CS_LOGIN payloads (25 packets) | ✅ Done | Sufficient for Phase 2 (TLoginSvr port). Methodology section covers the rest. |
| §4c CS_MAP payloads (713 packets) | ⏸ Deferred | Extract per-packet during Phase 4 (TMapSvr port). Most have inline comments in `CSProtocol.h`. |
| §4d Inter-server topology + dwKEY trust model | ✅ Done | TLoginSvr is a leaf — no SS sending → Phase 2 simplified |
| §4d MW/DM/RW/CT/SM payloads | ⏸ Deferred | Per-phase extraction. Inventory done. |
| §4e DB schema (reconstructed) | ⚠️ Partial | Table list + SP catalog complete. **Need MSSQL dump from user** for exact types/indexes. |

**Bottom line for next step (Phase 1 / Phase 2):** we have everything needed to start the .NET solution skeleton and port TLoginSvr. The protocol contract is locked-in; DB schema can be filled in lazily as we hit specific tables.

## Table of contents

- [1. Frame format](#1-frame-format)
- [2. Encryption — three layers](#2-encryption--three-layers)
- [3. Session state machine](#3-session-state-machine)
- [4. Ports & process map](#4-ports--process-map-servertnetlibtnetdefh)
- [4b. Packet ID catalog](#4b-packet-id-catalog)
- [4c. Login flow (CS_LOGIN)](#4c-login-flow-cs_login-packets--fully-documented-for-phase-2)
- [4d. Inter-server topology + dwKEY trust](#4d-inter-server-topology--login-trust-model)
- [4e. Database schema](#4e-database-schema-reconstructed-from-dbaccessh-files)
- [5. Open questions](#5-open-questions-to-resolve-before-writing-c-code)
- [6. C# implementation plan](#6-c-implementation-plan-preview)

Sources of truth (legacy C++):
- `Server/TNetLib/Packet.h` / `Packet.cpp` — frame format + XOR layer
- `Server/TNetLib/Session.cpp` — encryption orchestration, key table, RC4 layer
- `Server/TNetLib/CryptographyExt.cpp` — Win32 CryptoAPI wrapper (RC4)
- `Server/TNetLib/TNetDef.h` — port assignments
- `Lib/Own/TProtocol/include/*.h` — 1542 packet ID definitions + inline payload docs
- `Server/*/DBAccess.h` — schema & stored procedure surface

---

## 1. Frame format

Every packet starts with a fixed-size 16-byte header followed by an optional payload.

```
offset  size  field        notes
------  ----  -----------  ----------------------------------
0       2     wSize        Total packet size INCLUDING header. Always plaintext on wire (needed for framing before any decryption).
2       2     wID          Message ID. Encrypted (XOR + RC4 on C→S).
4       4     dwNumber     Sequence number. Per-direction, starts at 1, incremented every packet. Used for replay protection AND for selecting the XOR key.
8       8     llChkSUM     INT64 checksum of plaintext body. Verified after decrypt; mismatch = reject.
16      N     payload      Variable-length, see message catalog.
```

- Byte order: **little-endian** (x86/x64 Windows).
- Total size limits: `PACKET_HEADER_SIZE = 16`, `DEF_PACKET_SIZE = 1024` (initial buffer), `MAX_PACKET_SIZE = 0xFFFF`.

### Payload serialization primitives

The C++ side uses `operator<<` / `operator>>` overloads that write raw memory of each primitive (no varints, no length prefixes for fixed types):

| C++ type   | Wire bytes | C# equivalent          |
|------------|------------|-------------------------|
| `BYTE`     | 1          | `byte`                 |
| `char`     | 1          | `sbyte`                |
| `WORD`     | 2 LE       | `ushort`               |
| `short`    | 2 LE       | `short`                |
| `DWORD`    | 4 LE       | `uint`                 |
| `int`      | 4 LE       | `int`                  |
| `long`     | 4 LE       | `int` (MSVC `long` = 32-bit!) |
| `float`    | 4 LE IEEE  | `float`                |
| `__int64`  | 8 LE       | `long`                 |
| `LPCTSTR`  | `int` length prefix, then `length` bytes of raw `char` (no NUL). **MBCS, not UTF-16** — `_T` macro maps to `char` in this build (`#define NO_WARN_MBCS_MFC_DEPRECATION`). Likely Windows-1252 / system codepage. **Critical**: confirm with packet captures. | length-prefixed `byte[]` decoded with `Encoding.GetEncoding(1252)` (TBD) |
| `CString`  | Same as `LPCTSTR` on read. | same |
| Binary blob (`AttachBinary`) | `int` length prefix, then raw bytes. | length-prefixed `byte[]` |

> ⚠️ **MSVC `long` = 32-bit**, unlike .NET `long` (64-bit). Map `long` → `int` in C#.
> ⚠️ Strings have no NUL terminator on the wire; the length prefix is `int` (4 bytes LE), so theoretically can be negative — receiver bounds-checks via `CanRead`.

---

## 2. Encryption — three layers

The protocol is **asymmetric** and direction-dependent. Two crypto layers can be combined depending on session type.

### 2.1 Layer A: XOR body + header obfuscation (always when `m_bUseCrypt=true`)

After `m_bUseCrypt` is enabled (post-handshake), every outbound packet is processed in this order:

1. Increment per-session `m_dwSendNumber` and store into `header.dwNumber`.
2. Select key: `key = g_4skey[dwNumber % 7]`.
3. **Encrypt body** (`CPacket::Encrypt`):
   - body = bytes after header. `body_len = wSize - 16`.
   - Split into `n = body_len / 8` INT64 chunks + `r = body_len % 8` remainder bytes.
   - For each INT64 chunk `i`:
     ```
     checksum ^= body[i]      # checksum over PLAINTEXT
     body[i]  ^= key          # encrypt in place
     ```
   - For each remainder byte `i ∈ [0, r)`:
     ```
     checksum ^= body_left[i]                       # plaintext
     body_left[i] ^= ((byte*)&key)[i]               # encrypt with i-th byte of key
     crc = ((crc >> 4) & 0x0FFD) ^ key              # rolling INT64
     checksum += crc                                # bias the checksum
     ```
   - Write `checksum` into `header.llChkSUM`.
4. **Obfuscate header** (`CPacket::EncryptHeader`) — XOR bytes 2..15 of header (skipping `wSize`):
   - Let `wSize = header.wSize` (plaintext), `wID = header.wID` (still plaintext at this point — saved into a local before the loop).
   - For `i = 0..13`, byte `header[2 + i]`:
     - `i < 2` (the two bytes of `wID`): `byte ^= (byte)(key + wSize + i)`
     - `i >= 2` (bytes of `dwNumber` and `llChkSUM`): `byte ^= (byte)(key + wID + i)`

> Note: `wSize` is **never** modified by Layer A, so framing on the wire works before any decryption.

### 2.2 Layer B: RC4 outer (client→server only)

When the **client** is sending to the server, an additional RC4 stream-cipher pass wraps the already-XOR-encrypted packet:

```
key_material = "A5$$8AFS13A1::-11#!..'<U+2019>1716AC&<U+201D>/D1;;1#"
hashed_key   = MD5(key_material as bytes)
RC4_key      = derived from hashed_key via Win32 CryptDeriveKey(CALG_RC4)
ciphertext   = RC4(plaintext) over the ENTIRE 16-byte header + body
```

`g_strSecretKey` in `Session.cpp:16` contains non-ASCII characters (curly quotes `'` and `"`). The C++ build uses MBCS, so these are encoded as Windows-1252 single bytes. **TODO**: confirm the exact bytes via hex dump of `Session.obj` or a memory capture; do NOT trust the source-file rendering — IDE encoding may have altered them.

```
g_strSecretKey bytes (provisional, Windows-1252):
  41 35 24 24 38 41 46 53 31 33 41 31 3A 3A 2D 31 31 23 21 2E 2E 27 92 31 37 31 36 41 43 26 94 2F 44 31 3B 3B 31 23
   A  5  $  $  8  A  F  S  1  3  A  1  :  :  -  1  1  #  !  .  .  '  '  1  7  1  6  A  C  &  "  /  D  1  ;  ;  1  #
```

Length passed to `CryptHashData` = `(GetLength() + 1) * sizeof(TCHAR)` = `(38 + 1) * 1 = 39` bytes including trailing NUL.

> ⚠️ Layer B is **applied to the whole packet, including `wSize`**. But Section 1 said `wSize` must be plaintext for framing! This is a known oddity — needs a real packet capture to resolve. Hypotheses:
>   - The client implementation of `Encrypt` differs from server's `Decrypt` and skips wSize.
>   - The framing layer somehow tolerates encrypted wSize (unlikely).
>   - There is a separate per-byte length frame outside the encrypted packet.
> **Action**: capture a real login packet from the legacy server and dump bytes.

### 2.3 Receive flow (server-side)

```
read at least 16 bytes
size = u16(buf[0..2])              # framing uses plaintext wSize
read until 'size' bytes total

if session_type == CLIENT:         # peer is a client
    Decrypt():
        m_dwRecvNumber++
        key = g_4skey[m_dwRecvNumber % 7]
        # Layer B reverse:
        RC4_decrypt_in_place(buf, size)
        # Layer A reverse:
        DecryptHeader(key)
        if header.dwNumber != m_dwRecvNumber: REJECT
        DecryptBody(key) -> recomputes checksum on plaintext, compares to header.llChkSUM
        if mismatch: REJECT
else: # SESSION_SERVER (peer is another server)
    # No decryption at all. Inter-server is plaintext.
    accept as-is
```

### 2.4 Send flow (server-side)

```
if m_bUseCrypt:
    header.dwNumber = ++m_dwSendNumber
    key = g_4skey[header.dwNumber % 7]
    EncryptBody(key)        # XOR + checksum
    EncryptHeader(key)      # header obfuscation, skipping wSize
# No RC4 on outbound from server. Inter-server when m_bUseCrypt=false stays plaintext.
write packet to socket
```

### 2.5 The XOR key table (`g_4skey[KEY_COUNT]`, `KEY_COUNT=7`)

```c
0x5193817ae183acee
0x3891aeacbed18ead
0x549aeced13de13a1
0x09aeb1498c1eade9
0x19861acea1720ae7
0x0139aecea89541a2
0x6b97253c5fbb8b06
```

Indexed by `dwNumber % 7`. Note: `dwNumber=0` is never used (counter pre-increments), so the first packet uses index 1 (`0x3891aeacbed18ead`).

---

## 3. Session state machine

- `m_bUseCrypt` defaults to **false** on a fresh session — initial handshake packets are sent/received in plaintext.
- Some handler enables `m_bUseCrypt = true` after the handshake completes (location TBD — likely in TLoginSvr's `CSHandler` upon successful auth challenge). After enabling, the next outgoing packet uses `dwNumber=1` (since `++m_dwSendNumber` from 0).
- `m_dwSendNumber` and `m_dwRecvNumber` are independent (per direction).
- Replay attack mitigation: any out-of-order `dwNumber` rejects the packet and (typically) closes the session.

**TODO**: find the call site that flips `m_bUseCrypt = true` to document the handshake exactly.

---

## 4. Ports & process map (`Server/TNetLib/TNetDef.h`)

| Port  | Server         | Role                        |
|-------|----------------|-----------------------------|
| 3615  | TControlSvr    | Admin/control               |
| 3715  | TPatchSvr      | Patcher                     |
| 3815  | TWorldSvr      | World (guilds, parties, BR/BoW coordination) |
| 4815  | TLoginSvr      | Login / auth                |
| 5815  | TMapSvr        | Map / gameplay              |
| ?     | TLogSvr        | UDP logging (not in TNetDef) |
| ?     | TBRSvr         | Battle Royale               |
| ?     | TBoWSvr        | Battle of War               |

---

## 4b. Packet ID catalog

All packet IDs (`wMsgID`) are defined in `Lib/Own/TProtocol/include/*.h` as `#define NAME (BASE + 0xNNNN)`. The bases (`Lib/Own/TProtocol/include/ProtocolBase.h`):

```c
#define TVERSION    ((WORD) 0x2918)  // Protocol version (client sends this to gate compatibility)
#define SM_BASE        (0x1581)      // System messages (server-internal queue, NEVER seen on wire)
#define MW_BASE        (0x9001)      // Map server ↔ World server
#define DM_BASE        (0x5891)      // Database manager messages
#define CS_LOGIN       (0x1987)      // Login server ↔ Client
#define CS_MAP         (0x5280)      // Map server ↔ Client
#define CT_CONTROL     (0x9301)      // Control Server ↔ Admin client
#define CT_PATCH       (0x4201)      // Patch Server ↔ Client
#define RW_RELAY       (0x9999)      // Relay/World ↔ World coordination
#define CS_CUSTOM      (0x3312)      // Custom/extension messages

// Modern additions (post-modernization), NOT in legacy:
#define CT_PEER        (0x9F00)      // Peer self-registration extension
                                     // (TControlSvrAsio only — TPeer/Asio
                                     // servers self-register here)
```

### CT_PEER namespace (modern, no legacy peer)

Foundation for the modern cluster control plane. Peer game servers
(TLogin / TLog / TPatch / TMap) dial TControlSvrAsio on startup and
announce themselves; TControl issues a lease epoch; the peer keeps
it alive with a 30 s heartbeat; the lease-expiry sweep reaps anything
that misses ~3 windows.

Allocated outside the legacy 0x93xx range (highest legacy CT_\* id is
0x9382) to make it obvious these are not part of the 4Story client
wire surface. Wire framing reuses the existing CPacket header (8-byte
`WORD wSize | WORD wID | DWORD dwChkSum` + XOR-fold checksum + body),
so the same `ControlSession` codec drives them.

| ID | Name | Direction | Body |
|---|---|---|---|
| 0x9F00 | `CT_PEER_REGISTER_REQ`    | peer → control | `DWORD sid, CString name, CString addr, WORD port, CString version, DWORD pid, QWORD start_unix` |
| 0x9F01 | `CT_PEER_REGISTER_ACK`    | control → peer | `BYTE accepted, DWORD reason, QWORD lease_epoch, DWORD heartbeat_interval_sec` |
| 0x9F02 | `CT_PEER_HEARTBEAT_REQ`   | peer → control | `DWORD sid, QWORD lease_epoch, DWORD cur_users, DWORD max_users` |
| 0x9F03 | `CT_PEER_HEARTBEAT_ACK`   | control → peer | `BYTE accepted, QWORD lease_epoch` |
| 0x9F04 | `CT_PEER_DEREGISTER_REQ`  | peer → control | `DWORD sid, QWORD lease_epoch` |

Implementation:
* Server side — `Server/TControlSvrAsio/handlers/handlers_registry.cpp`
* Client side — `Lib/Own/FourStoryCommon/src/cluster/peer_client.cpp`

**Known security gap**: the handlers accept any caller that can
speak the framing. No IP allowlist, no PSK, no HMAC, no mTLS today.
See `CONTROL_SERVER_PORT_PLAN.md` §"Security gap" for the planned
fix (IP allowlist from TIPADDR + per-service PSK + HMAC-SHA256
trailer on every peer-side frame).

### Totals (extracted by `extract-packet-ids.ps1`, output `packet-ids.csv`)

| Prefix | Count | Notes |
|--------|------:|-------|
| **CS_** | 740   | Client ↔ Login + Client ↔ Map. The bulk of the game protocol. |
| **MW_** | 409   | Map ↔ World server-to-server traffic. |
| **DM_** | 209   | Database manager — likely TLoginSvr internal (`DBAccess` dispatch). |
| **CT_** | 122   | Control + Patch — admin/patcher traffic. |
| **SM_** |  45   | Internal queue messages, **not on wire**. Don't need C# wire mapping. |
| **RW_** |  17   | Relay/World coordination. |
| **Total** | **1542** | |

### Naming convention

- `*_REQ` — request (initiator → receiver)
- `*_ACK` — acknowledgement/response (receiver → initiator)
- Many pairs share a name root, e.g. `CS_LOGIN_REQ` (0x1988) and `CS_LOGIN_ACK` (0x1989).
- Some standalone events use `*_REQ` even when fire-and-forget.

### Cross-namespace ID collisions (⚠ document but plan around)

The base offsets overlap because the source uses additive `BASE + offset`. Most of the catalog has unique numeric IDs, but **9 collisions** were found by the extractor:

| Numeric ID | Packets | Resolution |
|-----------|---------|------------|
| `0x5944` | `DM_POSTBILLUPDATE_ACK` ↔ `DM_GETCHARINFO_REQ` | Same namespace — likely a source bug. Investigate which is canonical. |
| `0x9178` | `MW_CMGIFTRESULT_REQ` ↔ `MW_CMGIFTRESULT_ACK` | Intentional? Same handler dispatches both? Disambiguate by direction. |
| `0x9179` | `MW_ASSISTANT_REQ` ↔ `MW_ASSISTANT_ACK` | Same — direction-disambiguated. |
| `0x917A` | `MW_ASSISTANTANSWER_REQ` ↔ `MW_ASSISTANTANSWER_ACK` | Same. |
| `0x9181` | `MW_ASSISTANTDEL_REQ` ↔ `MW_ASSISTANTDEL_ACK` | Same. |
| `0x9208` | `MW_BRTEAMMATEDEL_REQ` ↔ `MW_ADDBRTEAMS_ACK` | Different ops! Source bug. |
| `0x55F0` | `CS_RELEASEFORBOW_REQ` ↔ `CS_PINGMEASUREMENT_REQ` | Both `_REQ` — clear bug. Verify which is used by reading handlers. |
| `0x55F1` | `CS_CASHBOWRESPAWN_REQ` ↔ `CS_PINGMEASUREMENT_ACK` | Different roles, but coexist. |
| `0x55F8` | `CS_LEAVEBATTLEFIELD_REQ` ↔ `CS_LEAVEBATTLEFIELD_ACK` | Same name — direction-disambiguated. |

**Action for C#:** The C# `MessageId` enum can't enforce uniqueness for genuine collisions. Two options:
1. Keep IDs unique by appending direction (`MessageId.MW_CMGIFTRESULT_REQ` vs `MessageId.MW_CMGIFTRESULT_ACK` → same `ushort` value but different enum members — won't compile).
2. Split into two enums per direction: `ClientToServerId` and `ServerToClientId`. Cleaner, matches actual dispatch reality.
3. **Preferred**: one enum but resolve collisions in source first (fix the legacy bugs) and treat REQ/ACK same-ID pairs as a single member.

### Where to find each catalog

Full table: [`packet-ids.csv`](packet-ids.csv) (`Id`, `IdDec`, `Name`, `Base`, `Offset`, `File`, `Line`).

Source files (legacy):
- `Lib/Own/TProtocol/include/SSProtocol.h` (45)
- `Lib/Own/TProtocol/include/CTProtocol.h` (139)
- `Lib/Own/TProtocol/include/DMProtocol.h` (209)
- `Lib/Own/TProtocol/include/MWProtocol.h` (409)
- `Lib/Own/TProtocol/include/CSProtocol.h` (740)

### Re-running the extractor

```powershell
cd _rewrite\docs
.\extract-packet-ids.ps1
```

The header files contain inline payload documentation as C++ comments — those are the source-of-truth for Task #3 (CS payload structures) and Task #4 (SS payload structures). Example from `CTProtocol.h`:
```c
#define CT_USERPOSITION_REQ    (CT_CONTROL + 0x0043)
// BYTE     bWorld
// STRING   strTargetName
// STRING   strGMName
```
Many — but not all — packets have these comments. For undocumented packets we'll read the corresponding `*Handler.cpp` and decode the `packet >>` / `packet <<` operator chain.

---

## 4c. Login flow (CS_LOGIN packets — fully documented for Phase 2)

The original C++ headers embed payload structure as inline comments next to each `#define`. For Phase 0 we extracted only the **CS_LOGIN namespace** (25 packets, the minimum needed to port TLoginSvr in Phase 2). CS_MAP payloads (~713 packets) and inter-server payloads will be extracted on-demand during their respective phases — the methodology is at the end of this section.

### High-level login handshake

```
Client                          TLoginSvr                       TMapSvr
  │                                 │                              │
  │── CS_LOGIN_REQ (creds) ─────────►│                              │
  │◄── CS_LOGIN_ACK (key, mapIP) ───│                              │
  │── CS_GROUPLIST_REQ ─────────────►│                              │
  │◄── CS_GROUPLIST_ACK (worlds) ───│                              │
  │── CS_CHANNELLIST_REQ (group) ──►│                              │
  │◄── CS_CHANNELLIST_ACK ──────────│                              │
  │── CS_CHARLIST_REQ (group) ─────►│                              │
  │◄── CS_CHARLIST_ACK (chars) ─────│                              │
  │── CS_CREATECHAR_REQ ───────────►│   (optional)                 │
  │◄── CS_CREATECHAR_ACK ───────────│                              │
  │── CS_START_REQ (group, char) ──►│                              │
  │◄── CS_START_ACK (mapIP, port) ──│                              │
  │  (disconnect from TLoginSvr)    │                              │
  │                                                                │
  │────────── CS_CONNECT_REQ (dwKEY) ─────────────────────────────►│
  │◄────────── CS_CONNECT_ACK ─────────────────────────────────────│
  │◄────────── CS_CHARINFO_ACK (full character state) ─────────────│
  │     (game traffic begins)                                      │
```

The `dwKEY` returned in `CS_LOGIN_ACK` is the **single-use authentication token** that TLoginSvr issues and TMapSvr validates — this binds the handoff. TMapSvr learns about valid keys via inter-server traffic (likely a `MW_*` packet from TLoginSvr to TWorldSvr to TMapSvr — confirm in Task #4).

### Packet payloads — CS_LOGIN namespace

> Notation: types per [§1 wire primitives](#payload-serialization-primitives). Nested blocks `{ … }` are repeated `count` times where `count` is the preceding byte. Strings are length-prefixed (int32) MBCS bytes.

#### `CS_LOGIN_REQ` (0x1988) — Client → TLoginSvr

| Field | Type | Notes |
|-------|------|-------|
| `wVersion` | WORD | Must match `TVERSION = 0x2918`. Mismatch → server rejects with `CS_LOGIN_ACK.bResult = version-error`. |
| `strUserID` | STRING | Account ID. |
| `strPasswd` | STRING | Plaintext password (!). After legacy port, replace with challenge-response. |

#### `CS_LOGIN_ACK` (0x1989) — TLoginSvr → Client

| Field | Type | Notes |
|-------|------|-------|
| `bResult` | BYTE | 0 = OK, otherwise error code (see `NetCode.h`). |
| `dwUserID` | DWORD | Server-assigned numeric account ID. |
| `bCreateCnt` | BYTE | Remaining character slots. |
| `dwCharID` | DWORD | Last-used char ID (auto-select). |
| `dwKEY` | DWORD | One-time auth token for TMapSvr. |
| `dwMapIP` | DWORD | TMapSvr IP (network byte order — verify). |
| `wPort` | WORD | TMapSvr port. |
| `bCharCreateCount` | BYTE | Total chars allowed. |
| `bInPcBang` | BYTE | Korean PC-room flag (legacy — keep at 0 outside KR). |
| `dwPremium` | DWORD | Premium account flags/expiration. |
| `dCurrentTime` | __int64 (`time64_t`) | Server timestamp, seconds since 1970. |

#### `CS_GROUPLIST_REQ` (0x198A) — Client → TLoginSvr — *no payload*

#### `CS_GROUPLIST_ACK` (0x198B) — TLoginSvr → Client

| Field | Type | Notes |
|-------|------|-------|
| `bCount` | BYTE | Number of world groups. |
| repeated `bCount` times: |
| `strName` | STRING | World display name. |
| `bGroupID` | BYTE | Internal ID. |
| `bType` | BYTE | World type/category. |
| `bStatus` | BYTE | Online/full/maintenance. |
| `bCharCnt` | BYTE | This account's chars on this world. |

#### `CS_CHANNELLIST_REQ` (0x198C) — Client → TLoginSvr

| Field | Type |
|-------|------|
| `bGroupID` | BYTE |

#### `CS_CHANNELLIST_ACK` (0x198D) — TLoginSvr → Client

| Field | Type | Notes |
|-------|------|-------|
| `bCount` | BYTE | |
| repeated: |
| `strName` | STRING | Channel display name (e.g., "Channel 1"). |
| `bChannel` | BYTE | Channel ID. |

#### `CS_CHARLIST_REQ` (0x198E) — Client → TLoginSvr

| Field | Type |
|-------|------|
| `bGroupID` | BYTE |

#### `CS_CHARLIST_ACK` (0x198F) — TLoginSvr → Client

```
BYTE bCount
{
  DWORD dwID                       // char id
  STRING strName
  BYTE bSlotID                     // 0..MAX_CHARSLOT-1
  BYTE bLevel
  BYTE bClass                      // job/class enum
  BYTE bRace                       // race enum
  BYTE bCountry                    // faction
  BYTE bSex
  BYTE bHair, bFace, bBody, bPants, bHand, bFoot   // appearance
  DWORD dwRegionID                 // current region
  BYTE bEquipItemCount
  {
    BYTE bItemID                   // inventory slot enum
    WORD wItemID                   // item template id
    BYTE bLevel
    BYTE bGradeEffect              // visual grade
  }
}
```

#### `CS_CREATECHAR_REQ` (0x1990) — Client → TLoginSvr

```
BYTE bGroupID
STRING strName
BYTE bSlotID
BYTE bClass, bRace, bCountry, bSex
BYTE bHair, bFace, bBody, bPants, bHand, bFoot
```

#### `CS_CREATECHAR_ACK` (0x1991) — TLoginSvr → Client

```
BYTE bResult                       // 0=ok else error
DWORD dwID                         // new char ID
STRING strName
BYTE bSlotID
BYTE bClass, bRace, bCountry, bSex
BYTE bHair, bFace, bBody, bPants, bHand, bFoot
BYTE bCreateCnt                    // remaining slots
```

#### `CS_DELCHAR_REQ` (0x1992) — Client → TLoginSvr

```
BYTE bGroupID
STRING strPasswd                   // requires re-auth to delete
DWORD dwCharID
```

#### `CS_DELCHAR_ACK` (0x1993) — TLoginSvr → Client

```
BYTE bResult
DWORD dwCharID
```

#### `CS_START_REQ` (0x1994) — Client → TLoginSvr

```
BYTE bGroupID
BYTE bChannel
DWORD dwCharID
```

#### `CS_START_ACK` (0x1995) — TLoginSvr → Client

```
BYTE bResult
DWORD dwMapIP                      // final TMapSvr endpoint (could differ from CS_LOGIN_ACK due to channel)
WORD wPort
BYTE bServerID
```

#### Test / version probe — `CS_TESTLOGIN_REQ` (0x1996), `CS_TESTVERSION_REQ` (0x1998), `CS_TESTVERSION_ACK` (0x1999)

No payload documented in headers; investigate handler for stress-test tooling. Probably non-essential — skip in Phase 2.

#### `CS_AGREEMENT_REQ` (0x199A) — Client → TLoginSvr

```
WORD wVersion                      // EULA/version acceptance
```

#### `CS_HOTSEND_REQ` / `_ACK` (0x199B, 0x199C) — Keepalive ping. Empty payload. Client must respond to maintain session.

#### `CS_VETERAN_REQ` / `_ACK` (0x199D, 0x199E) — "Returning veteran player" reward eligibility. Payload undocumented in header — read `CSHandler.cpp:OnCS_VETERAN_*` during Phase 2.

#### `CS_SECURITYCONFIRM_REQ` (0x199F), `CS_SECURITYCONFIRM_ACK` (0x19A0), `CS_SECURITYRESULT_ACK` (0x19A7) — Anti-cheat (HShield/XTrap/HwidLib) handshake. Payload not in header — these wrap `HwidManagerSvr` state. **Defer to Phase 2** with mockable stub (the rewrite probably won't keep HShield/XTrap; modern equivalent like EAC/BattlEye or just server-side validation).

### Methodology for extracting remaining payloads (CS_MAP, MW, DM, SS, RW, CT)

For each packet not yet documented:

1. **Look at the `#define` site** in `Lib/Own/TProtocol/include/*.h`. ~80% of packets have inline comments above the define with the payload struct.
2. **If not commented**, locate the handler with:
   ```powershell
   # Find sender:
   Select-String -Path 'Server\**\*Sender.cpp' -Pattern 'Send<PACKET_NAME>|SetID\s*\(\s*<PACKET_NAME>'
   # Find handler:
   Select-String -Path 'Server\**\*Handler.cpp' -Pattern 'On<PACKET_NAME>'
   ```
3. **Read the `packet >> field;` chain** in the handler. Each `>>` reads one primitive in order — that's the payload struct.
4. **Verify with the sender**: matching `packet << field;` writes.
5. **Validate against client** (when client gets ported): hex-diff a real captured packet vs C# serialized output.

### Korean comment hygiene

Many inline comments are mojibake (Korean text in CP949 misread as CP1252). They're harmless but unreadable — don't waste effort translating, just preserve field names. Examples:
- `// 12m10d` near `bStartAct` likely means "added 2012-10". Treat as version annotation.
- `// 0316 ����` means "added 03/16" + Korean word. Skip translating.

### What's NOT in scope for Phase 0

- Full payload transcription for all 713 CS_MAP packets — **deferred to Phase 4**.
- Full payload transcription for MW/DM — **deferred to per-phase** (port the consumer first, then RE the payloads needed).
- TLogSvr UDP log packets (`LogPacket.h` 373 lines, 79 defines) — **deferred to Phase 3**.

---

## 4d. Inter-server topology + login trust model

### Process roles & message direction

| Server      | Has CSHandler | Has CSSender | Has SSHandler | Has SSSender | Has RW    | Has DM     |
|-------------|:-:|:-:|:-:|:-:|:-:|:-:|
| TLoginSvr   | ✅ | ✅ | ✅ (receive only) | ❌ | ❌ | ❌ |
| TWorldSvr   | ❌ | ❌ | ✅ | ✅ | ✅ Handler+Sender | ✅ DMSender (writes only) |
| TMapSvr     | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| TControlSvr | ❌ | ❌ | ✅ | ✅ | ❌ | ❌ |
| TPatchSvr   | mostly file transfer, simpler protocol |
| TBRSvr/TBoWSvr | specialized event servers, attached to TWorldSvr |

**Key observation**: TLoginSvr is a **leaf** in the inter-server graph — it only receives SS messages (server status/heartbeats from TWorldSvr-side coordinators) but **never sends MW/SS/DM**. This means a Phase 2 .NET port of TLoginSvr does NOT need to implement any inter-server sending. ✅ Big simplification.

### `dwKEY` session-token trust model

The mystery from Section 4c (how does TMapSvr learn a `dwKEY` is valid?) is now resolved. The original implementation does **NOT propagate `dwKEY` from TLoginSvr to other servers**. Instead:

```
1. Client → TLoginSvr:  CS_LOGIN_REQ(creds)
2. TLoginSvr:           authenticate against DB, generate random dwKEY
3. TLoginSvr → Client:  CS_LOGIN_ACK(dwKEY, mapIP, mapPort)
   ── (TLoginSvr does NOT tell any other server about dwKEY) ──
4. Client → TMapSvr:    CS_CONNECT_REQ(charID, dwKEY, ipAddr, port)
5. TMapSvr → TWorldSvr: MW_ADDCHAR_REQ(charID, dwKEY, ipAddr, port, userID)
6. TWorldSvr OnMW_ADDCHAR_ACK:
   - If charID NOT in m_mapTCHAR:  register, store dwKEY, accept.  ← TRUST ON FIRST USE
   - If charID already present:     verify dwKEY matches existing record.
                                    Mismatch → MW_INVALIDCHAR_REQ + kick.
7. TWorldSvr → TMapSvr: MW_ENTERSVR_REQ → TMapSvr finishes auth, sends CS_CONNECT_ACK
```

(Source: `Server/TWorldSvr/SSHandler.cpp:704` `OnMW_ADDCHAR_ACK`.)

#### Trust assumptions
- **Inter-server network is trusted** (private LAN, firewalled). MW/SS traffic has no auth, it's plaintext (m_bUseCrypt=false).
- **`dwKEY` is a 32-bit opaque token** generated by TLoginSvr. With ~4 billion possibilities, brute force is impractical at MMORPG scale.
- **Trust-on-first-use at TWorldSvr**: this is a real weakness. Theoretically an attacker who guesses a valid `charID` could connect with a random `dwKEY` before the legitimate user, hijacking the slot. Mitigated in practice because `dwKEY` is unguessable, and the legit client connects to TMapSvr within milliseconds of receiving it.

#### Rewrite recommendation
For the .NET port, **fix this** during Phase 2 by making TLoginSvr push session tokens to a shared store (Redis or PostgreSQL) before responding `CS_LOGIN_ACK`. TMapSvr (or its Orleans grain equivalent) verifies the token against the store on `CS_CONNECT_REQ` — eliminates trust-on-first-use. Use a 128-bit token (Guid) wire-compatible by padding to two DWORD fields, or extend the protocol with a new field once client is also rewritten.

### MW protocol — key login-path packets

These are the ones TWorldSvr handles during character entry (full list of 409 in `packet-ids.csv`). Detailed payloads deferred to Phase 3 (TWorldSvr port). For Phase 2 (TLoginSvr) we do not need to implement any of these.

| ID | Name | Direction | Purpose |
|----|------|-----------|---------|
| 0x9001+0x... | `MW_CONNECT_REQ/ACK` | TMapSvr → TWorldSvr (register map server) | Map server announces itself; TWorldSvr replies with current game state (events, cash items, rankings) |
| | `MW_ADDCHAR_ACK` | TMapSvr → TWorldSvr | New client connecting to map; carries `(charID, dwKEY, IP, port, userID)` |
| | `MW_ENTERSVR_REQ/ACK` | TWorldSvr ↔ TMapSvr | Authorize character to enter; triggers char data load |
| | `MW_CHARDATA_REQ/ACK` | TWorldSvr ↔ TMapSvr | Full character snapshot (inventory, skills, etc.) |
| | `MW_CLOSECHAR_ACK` | TMapSvr → TWorldSvr | Client disconnected |
| | `MW_INVALIDCHAR_REQ` | TWorldSvr → TMapSvr | Kick suspicious connection |
| | `MW_CHECKMAIN_ACK` | TMapSvr → TWorldSvr | Validate "main" map server for char (multi-map char migration) |
| | `MW_MAPSVRLIST_ACK` | TWorldSvr → TMapSvr | Available map servers list |

### DM protocol — TWorldSvr → DB worker (one-way)

`Server/TWorldSvr/DMSender.cpp` exists but no corresponding `DMHandler.cpp` in TWorldSvr. This suggests TWorldSvr only **sends** DM packets — they're consumed by a separate DB worker process or by TLoginSvr's DB layer. Since TLoginSvr also has no `DMHandler.cpp`, the DM packets likely go through a custom dispatcher not yet located.

**TODO** (Phase 3): trace `DMSender` call sites and find the listener — could be in `SqlBase.cpp` async dispatch.

### RW protocol — Relay/World

`RW_RELAY` (base 0x9999, 17 packets) — between TWorldSvr and an optional **Relay** server (for inter-world chat, friend cross-server presence, etc.). Defined in `CTProtocol.h:286+`. Optional for single-world deployment — skip until needed.

### CT protocol — Control/Admin

`CT_CONTROL` (base 0x9301, 110 packets) — used by an admin GUI (probably a separate exe). Includes service start/stop, kick users, ban chat, monster spawn search, item find, GM messaging. Deferred to Phase 5+ unless private-server has an admin requirement.

`CT_PATCH` (base 0x4201, 12 packets) — patcher protocol. Simple file-transfer. Phase 3.

### SM protocol — server-internal queue messages

`SM_BASE = 0x1581`, 45 messages. These never appear on the wire — they're enqueued onto a server's batch thread for deferred processing (e.g., `SM_AICMD_REQ` queues an AI command, `SM_DUELSTART_REQ` initiates duel state machine). In the .NET rewrite, these map naturally to **Wolverine local commands** or **Orleans grain timer callbacks**. No protocol port needed.

---

## 4e. Database schema (reconstructed from `DBAccess.h` files)

Without access to a running MSSQL instance or a `.bak`/`.sql` dump, the schema below is reconstructed from `CSqlQuery` subclasses in `Server/{TLoginSvr,TWorldSvr,TMapSvr,TControlSvr,TPatchSvr}/DBAccess.h`. Column types are inferred from C++ field types — **exact SQL types (e.g. `VARCHAR(N)` vs `NVARCHAR(N)`, `BIGINT` vs `INT`), nullability, indexes, FKs are unknown** until a real dump is available.

### Database architecture: sharded by world

The original deployment uses **multiple databases**:

1. **Login/Master DB** — contains the world registry and account/auth tables:
   - `TGROUP` — world groups: `bGroupID, bType, szNAME, szDSN, szUserID, szPasswd, bStatus, wFull, wBusy, dwMaxUser`. **Each row's `szDSN` is the ODBC DSN of a per-world game DB** → sharding key.
   - `TCHANNEL` — channels: `bGroupID, bChannel, szNAME, bStatus, wFull, wBusy`
   - `TCURRENTUSER` — active sessions: `dwKEY, bGroupID, bChannel, dwUserID`
   - `TALLCHARTABLE` — char index across all worlds: `dwUserID, bWorldID, dwCharID, bDelete`
   - `TVETERANCHART` — veteran rewards: `bID, bLevel`
   - Accounts and HWID-banning tables (not directly seen, accessed via stored procs `TLogin`, `TCheckHWID`, `TGetBanReason`, etc.)

2. **Per-world Game DB** (one per `TGROUP` row) — contains character data, items, guilds, gameplay state.

For our private-server rewrite, **collapse this to a single PostgreSQL database** with a `world_id` column where the original code switched DSN. Simpler ops, lossless for single-shard deployments. Multi-shard support can be reintroduced later via `IDbContextFactory<T>` per-shard if needed.

### Tables / categories observed

#### Accounts & sessions (Login DB)
`TGROUP`, `TCHANNEL`, `TCURRENTUSER`, `TALLCHARTABLE`, `TVETERANCHART`

#### Characters (Game DB)
`TCHARTABLE` — main char row (columns: `dwUserID, dwCharID, szNAME, bStartAct, bClass, bRace, bCountry, bSex, bHair, bFace, bBody, bPants, bHand, bFoot, bSlot, bLevel, dwRegion, bHelmetHide, bDelete, dLogoutDate`)
`THOTKEYTABLE`, `TPROTECTEDTABLE`, `TVIEW_SOULMATE`, `TPOSTTABLE` (mail), `TAIDTABLE`

#### Items
`TITEMTABLE` (instances), `TITEMCHART` (templates), `TITEMATTRCHART`, `TITEMGRADECHART`, `TITEMSETCHART`, `TITEMMAGICCHART`, `TITEMMAGICSKILLCHART`, `TITEMUSEDTABLE`, `TGEMGRADECHART`, `TINVENTABLE`, `TCABINETTABLE`, `TINVENTOURNAMENTCHART`, `TITEMTOURNAMENTCHART`

#### Skills
`TSKILLTABLE`, `TSKILLCHART`, `TSKILLDATA`, `TSKILLMAINTAINTABLE`, `TSKILLPOINTCHART`, `TPREMIUMSKILLCHART`

#### Guilds & Tactics
`TGUILDTABLE`, `TGUILDCHART`, `TGUILDMEMBERTABLE`, `TGUILDTACTICSTABLE`, `TGUILDMASTERSKILLTABLE`, `TGUILDSKILLCHART`

#### Quests
`TQUESTTABLE`, `TQUESTCHART`, `TQCONDITIONCHART`, `TQREWARDCHART`, `TQUESTITEMCHART`, `TQUESTTERMCHART`, `TQUESTTERMTABLE`

#### Monsters & AI
`TMONSTERCHART`, `TMONATTRCHART`, `TMONITEMCHART`, `TMONSPAWNCHART`, `TAICHART`, `TAICMDCHART`, `TAICONCHART`, `TMONSTERSHOPCHART`, `TRECALLMONTABLE`, `TRECALLMAINTAINTABLE`

#### World / maps
`TMAPMONCHART`, `TSPAWNPATHCHART`, `TSPAWNPOSCHART`, `TPORTALCHART`, `TDESTINATIONCHART`, `TGATECHART`, `TSWITCHCHART`, `TLOCALTABLE`, `TBATTLEZONECHART`, `TINDUNCHART`, `TCASTLETABLE`, `TSKYGARDENTABLE`, `TNPCCHART`, `TNPCITEMCHART`, `TOPERATORTABLE`

#### Pets / Mounts / Companions
`TPETTABLE`, `TMOUNTCHART`, `TCOMPANIONTABLE`, `TCOMPANIONITEMTABLE`, `TCOMPANIONBONUSCHART`

#### Cash shop
`TVIEW_CASHCATEGORYCHART`, `TVIEW_CASHSHOPITEMCHART`, `TVIEW_CASHITEMCABINET`, `TVIEW_DURINGITEMTABLE`, `TEXPITEMTABLE`, `TGAMBLECHART`, `TVIEW_CASHGAMBLECHART`

#### PvP / Battle / Events
`TPVPRECENTTABLE`, `TPVPOINTCHART`, `TDUELSCORETABLE`, `TDUELCHARTABLE`, `TLOCALOCCUPYTABLE`, `TGODBALLCHART`, `TGODTOWERCHART`, `TMONTHRANKTABLE`, `THEROTABLE`, `TBATTLERANKCHART`, `TARENACHART`, `TMISSIONTABLE`, `TRPSGAMECHART`

#### Battle Royale / Battle of War
`TBOWITEMCHART`, `TBOWBONUSITEMCHART`, `TBRSPAWNPOSCHART`, `TBRSUPPLIESCHART`

#### Auction house
`TAUCTIONTABLE`, `TAUCTIONINTEREST`, `TAUCTIONBIDDER`

#### Static reference (mostly read-only `*CHART` tables)
`TFORMULACHART`, `TLEVELCHART`, `TCLASSCHART`, `TRACECHART`, `TTITLECHART`, `TTITLETABLE`, `TSVRCHART`, `TSVRMSGCHART`, `THELPMESSAGETABLE`, `TSPECIALBOXCHART`, `TCHANNELCHART`, `TEQUIPCREATECHARCHART`

### Stored procedures (TLoginSvr surface)

The login flow uses stored procedures rather than ad-hoc SQL:

| Procedure | Purpose | Inputs → Outputs |
|-----------|---------|------------------|
| `TLogin(userID, passwd, loginIP, ipCheck)` | Authenticate | → `dwKEY, dwCharID, dwID, szIPAddr, wPort, bCreateCnt, bInPcBang, dwPremium` |
| `TLoginJP(...)` | Japan-region variant with `bChanneling` | similar |
| `TLogout(id, charID, level, exp)` | Save session end | → result |
| `TCheckPasswd(id, passwd)` | Password verify | → result |
| `TCheckIP(ip)`, `TCheckHWID(hwid)`, `TCheckHwid(hwid)` | Ban checks | → result |
| `TGetBanReason(userID)` | Ban details | → `szReason, dwDuration, dUnbanTime, bEternal` |
| `TRoute(groupID, serverID, type)` | Find server endpoint | → `szIP, wPort` |
| `TLoadService(world, serviceGroup)` | Service discovery | → `szIP, wPort` |
| `TFindServerID(charID, channel)` | Resolve char's home server | → `bServerID` |
| `TCheckVeteran(userID)` | Returning-player check | → result |
| `TCreateChar(...)`, `TDeleteChar(...)` | Character lifecycle | |
| `TAddMacAddress`, `TGetClientSha`, `TLogLogin` | Anti-cheat instrumentation | |
| `TAgreement(userID)`, `TGetNation()` | Misc | |
| `TGetGuildInfo(charID)` | Guild summary | → `szName, dwFame, dwFameColor` |
| `TClearLoginCurrentUser()` | Boot-time cleanup of stale sessions | |
| `TFindBOWPlayer(userID)`, `TFindBRPlayer(userID)` | BoW/BR registration lookup | |

### Migration plan for PostgreSQL + EF Core

1. **Replace ALL stored procedures with EF Core LINQ queries** in C#. Reasons:
   - SP-in-DB couples logic to DB engine (T-SQL ↔ PL/pgSQL not portable).
   - Modern observability (OpenTelemetry) works better with queries the ORM emits.
   - Easier to evolve schema with code-first migrations.
2. **Schema-first conversion**: get an actual MSSQL `.bak` or `.sql` schema dump from the user, then write a one-off `dotnet ef migrations add Initial` after defining `DbContext` with `OnModelCreating` mapping the legacy table/column names (preserve `T*` prefixes initially for diffability).
3. **Naming convention**: keep legacy table/column names in `[Table("TCHARTABLE")]` / `[Column("szNAME")]` for now. Rename in a later migration once everything works.
4. **Type mapping**:
   - `TCHAR[]` (Windows-1252 MBCS) → `text` (UTF-8 in Postgres) — needs **transcoding pass** on initial data import.
   - `DWORD` → `bigint` (signed, since Postgres has no unsigned 32-bit).
   - `WORD` → `integer` (signed, since Postgres has no unsigned 16-bit).
   - `BYTE` → `smallint`.
   - `TIMESTAMP_STRUCT` → `timestamp without time zone`.
   - `INT64 / __time64_t` → `bigint` (Unix seconds for `time64_t`).
5. **Encoding migration of strings**: Korean comments are CP949 mojibake under CP1252. Actual game text fields (player names, guild names, NPC dialogues) might be in CP1252, CP949, or mixed depending on origin. Test with a real DB before assuming.

### ✅ Real schema extracted (2026-05-17)

User provided `TGAME_RAGEZONE.bak` + `TGLOBAL_RAGEZONE.bak`. Both restored locally and full schema dumped. **See [SCHEMA.md](SCHEMA.md) for authoritative details.**

Summary: 268 tables / 2405 columns / 294 indexes / 5 FKs / 323 stored procedures. The "inferred" architecture in §4e turned out to match reality. Notable real-world adjustments:
- TGAME has **zero foreign keys** (perf-driven choice — reproduce as EF Core navigation properties only, or add real FKs in PG).
- TLogin SP references a federated billing service at `192.168.1.9` (commented out) — original deployment was multi-process.
- 27 views in TGAME (not in earlier inference) — mostly read aggregations for the UI; can become EF Core projections.

---

## 5. Open questions to resolve before writing C# code

1. ✅ **Does the client RC4 the entire 16-byte header on send, or skip the first WORD (`wSize`)?** — RESOLVED 2026-05-17 via source code analysis (`Session.cpp:86-91`): RC4 spans the entire packet **including** wSize bytes; both sides save/restore wSize from a local variable so that the wire keeps it plaintext for framing. See `COMPLETENESS_ANALYSIS.md` §1. C# impl updated to use `Rc4Layer.TransformPacketPreservingWSize`.
2. **Exact bytes of `g_strSecretKey`** — confirm via `Session.obj` hex dump that the curly quotes are bytes `0x92` and `0x94` (CP1252) and not something else.
3. ✅ **String encoding on the wire** — RESOLVED 2026-05-17 via DB collation inspection: `Latin1_General_CI_AS_KS` → **CP1252**. Confirmed with byte-level dump of `TNPCCHART.szName` (all ASCII names like "Eilinora", no high bytes).
4. **`m_bUseCrypt = true` activation point** — which packet triggers it, and is the very next packet already encrypted or only after an ACK?
5. **Server-to-server `m_bUseCrypt`** — confirmed always false? Or does some inter-server handshake flip it?
6. **RC4 key size** (new, raised by `COMPLETENESS_ANALYSIS.md` §2) — assumed 128-bit (full MD5 hash); could be 40-bit (first 5 bytes only) if Win32 CSP returned an export-grade key. Validate against real captured packet at Phase 2.

---

## 6. C# implementation plan (preview)

`FourStory.Protocol` (`_rewrite/src/FourStory.Protocol/`):

```
FourStory.Protocol/
├── PacketHeader.cs            # [StructLayout(LayoutKind.Sequential, Pack=1)] readonly struct
├── PacketReader.cs            # ref struct over ReadOnlySpan<byte>, primitives + strings
├── PacketWriter.cs            # IBufferWriter<byte>-based, mirrors C++ operator<<
├── MessageId.cs               # enum ushort (filled by next phase)
├── Crypto/
│   ├── XorLayer.cs            # Encrypt/Decrypt body + header obfuscation, INT64 checksum
│   ├── Rc4Layer.cs            # System.Security.Cryptography... NO. RC4 was removed from .NET; implement directly (~30 LOC).
│   └── KeyTable.cs            # g_4skey[], g_strSecretKey constant
├── Session/
│   ├── PacketCodec.cs         # PipeReader/PipeWriter-based framing + crypto orchestration
│   └── SessionType.cs         # enum { ClientFacing, ServerToServer }
└── FourStory.Protocol.csproj  # net10.0, AOT-friendly
```

Test strategy: **golden vectors**. Capture real packets from running legacy server (Wireshark or local loopback dump), check them into `FourStory.Protocol.Tests/golden/`, assert C# round-trip matches byte-for-byte.

---

*Last updated: 2026-05-17 (Phase 0, Task #1 — encryption + framing documented; packet ID catalog TBD)*
