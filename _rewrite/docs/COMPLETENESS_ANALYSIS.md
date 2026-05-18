# C++ → C# Completeness Analysis (Phase 1 Protocol Layer)

Date: 2026-05-17. Reviewer: line-by-line comparison of every protocol-layer operation.

## TL;DR

| Component | C++ source | C# implementation | Status |
|-----------|-----------|--------------------|--------|
| Wire frame format (16B header) | `Packet.h:36` | `PacketHeader.cs` | ✅ Match |
| Primitive serialization | `Packet.cpp` operator<< / >> | `PacketReader.cs` / `PacketWriter.cs` | ✅ Match |
| String wire format | `Packet.cpp:377` / `441` | `PacketReader.WriteString` / `ReadString` | ✅ Match |
| Body XOR encrypt + checksum | `Packet.cpp:38-67` | `XorLayer.EncryptBody` | ✅ Match |
| Body XOR decrypt + checksum verify | `Packet.cpp:84-117` | `XorLayer.DecryptBody` | ✅ Match |
| Header obfuscation (encrypt) | `Packet.cpp:69-82` | `XorLayer.EncryptHeader` | ✅ Match |
| Header de-obfuscation (decrypt) | `Packet.cpp:119-131` | `XorLayer.DecryptHeader` | ✅ Match |
| 7-key INT64 table | `Session.cpp:7-14` | `KeyTable.Keys` | ✅ Match |
| Outbound session encrypt | `Session.cpp:63-72` | `PacketCodec.Encrypt` | ✅ Match |
| **Inbound session decrypt — RC4 layer** | `Session.cpp:74-99` | `PacketCodec.TryDecrypt` | ❌ **BUG — see §1** |
| RC4 algorithm | `CryptographyExt.cpp` (Win32 CryptoAPI) | `Rc4Layer.cs` (hand-rolled) | ⚠️ Match in algorithm, key-derivation assumption unverified |
| Replay protection (dwNumber) | `Session.cpp:95-96` | `PacketCodec.TryDecrypt` | ✅ Match |
| TCP framing / IOCP | `Session.cpp` WSARecv path | `FourStory.Protocol.Session.PacketSession` | ✅ Implemented (async loop) |
| Buffer pool / reuse | `Packet.cpp::Flush/ExpandIoBuffer` | NOT IMPLEMENTED | 🟡 Future optimization |
| Per-session locking | `CCSLock` everywhere | NOT IMPLEMENTED | 🟡 Document or guard at `PacketSession` |

**Verdict: protocol layer faithful to the legacy. 3 protocol-flow bugs found in the 2026-05-17 Login/Map audit are now fixed — see §8.**

---

## 1. ❌ Critical bug: RC4 keystream alignment

### Symptom (theoretical, would manifest at first real client connection)
A C# server speaking the new codec would fail to decrypt the very first client packet, because the RC4 keystream is offset by 2 bytes relative to what the legacy client encodes.

### What C++ does (`Session.cpp:86-91`)
```cpp
DWORD dwBufSize = DWORD(pPacket->m_pHeader->m_wSize);

// RC4 transforms the ENTIRE packet starting from offset 0 (including wSize bytes)
if(!EncryptBuffer(CALG_RC4, pPacket->m_pBuf, dwBufSize,
                  pPacket->m_pBuf, dwBufSize, lpszSecretKey, dwSecretKey))
    return FALSE;

// wSize was just corrupted by RC4 — restore from local variable
pPacket->m_pHeader->m_wSize = WORD(dwBufSize);
```

### What C# previously did (`PacketCodec.TryDecrypt`)
```csharp
if (PeerType == PeerType.Client)
{
    Rc4Layer.TransformInPlace(packet[2..]);  // SKIPS wSize entirely
}
```

### Why this matters
RC4 is a stream cipher — each invocation produces a keystream byte-by-byte. The legacy client must also RC4 from offset 0 (otherwise the receiver could never recover the body correctly given the existing C++ server impl). Skipping wSize on the C# side means keystream byte 0 lands on packet[2] instead of keystream byte 2. **All decrypted body bytes would be XORed with a shifted-by-2 keystream → garbage.** The checksum validation would catch this every time.

### Fix (this commit)
1. `PacketCodec.TryDecrypt`: cache wSize bytes (offsets 0..1), RC4 the entire packet, restore wSize bytes.
2. Test helper `BuildClientPacket` (in `PacketCodecTests`): do the same on the client side so the test reproduces wire-faithful bytes.

The fix is in `Rc4Layer.cs` as a new helper `TransformPacketAtOffset0` that documents the wSize-preservation invariant, plus minor edits to `PacketCodec.TryDecrypt`.

### Resolved open question
PROTOCOL.md §5 Q1 ("Does the client RC4 include wSize?") — **YES, RC4 spans the full packet; wSize is plaintext on the wire only because both sides save/restore it.** Document update applied.

---

## 2. ⚠️ Unverified assumption: RC4 key size

### The Win32 call
```cpp
CryptCreateHash(hCryptProv, CALG_MD5, NULL, 0, &hCryptHash);
CryptHashData(hCryptHash, lpszSecretKey, dwSecretKey, 0);
CryptDeriveKey(hCryptProv, CALG_RC4, hCryptHash, CRYPT_EXPORTABLE, &hCryptKey);
```

The provider is `PROV_RSA_FULL` (acquired in `EncryptBuffer`). For RC4 with this provider:
- **Pre-XP-SP3 Windows**: 40-bit (5-byte) export-grade key (uses only first 5 bytes of MD5 hash, rest zeroed).
- **XP-SP3 and later**: 128-bit (16-byte) key by default.

The `CRYPT_EXPORTABLE` flag is about whether the key can be exported, not about key strength. With no explicit key-length bits in the upper 16 bits of `dwFlags`, the CSP picks its default.

### What C# assumes
`Rc4Layer.DefaultKey = MD5.HashData(KeyTable.RawSecretKey)` — full 16 bytes. **Assumes 128-bit RC4.**

### How to verify
1. Capture a real client→server packet from the legacy binary running on Windows.
2. Try decrypting with 128-bit key (current impl). If checksum validates → confirmed.
3. If not, try 40-bit key (first 5 bytes of MD5, rest as zeros).

For now: **leave at 128-bit, document the assumption, will discover at Phase 2 first integration test.**

---

## 3. Minor divergences (not bugs, but worth documenting)

### 3.1 `recvNumber` advancement on decrypt failure

- **C++** (`Session.cpp:79`): `m_dwRecvNumber++` happens BEFORE any checks. If decrypt fails, the counter has still advanced. Next packet's expected number is `recvNumber+1` (since it was already incremented). In practice this rarely matters — failed decrypt usually triggers session close.
- **C#** (`PacketCodec.TryDecrypt`): `_recvNumber = expectedNumber` only on success. On failure, the counter stays put.

**Impact**: For a normally-functioning session, identical behavior. The only observable difference is in a hypothetical "retry after failure" path, which doesn't exist in either codebase. Documented as a deliberate stylistic difference; no functional impact.

### 3.2 String read failure mode

- **C++** (`Packet.cpp:441-457`): `operator>>(CString&)` silently leaves the param empty if length exceeds remaining bytes.
- **C#** (`PacketReader.ReadString`): throws `InvalidOperationException` via `EnsureCanRead`.

**Impact**: C# is louder, which is preferred for catching protocol-level bugs early. Document for users porting handlers.

### 3.3 Header `wID` read inside loop (decrypt)

- **C++** `DecryptHeader` reads `m_pHeader->m_wID` inside the loop each iteration (line 129). This is correct because for `i>=2` the wID bytes have been XOR-revealed by iterations `i=0` and `i=1`.
- **C#** `DecryptHeader` re-reads `wId` from `header[2..4]` inside the loop too — same effect.

✅ Match in semantics, just makes the dependency on prior-iteration state explicit.

### 3.4 Pre-loop snapshot of `wID` in encrypt

- **C++** `EncryptHeader` stashes `WORD wID = m_pHeader->m_wID` BEFORE the loop. Necessary because as encrypt proceeds, the in-memory wID would become ciphertext.
- **C#** `EncryptHeader` reads `wId` once before the loop and uses the local. Same.

✅ Match.

---

## 4. 🟡 Gaps — known not implemented (planned)

| Gap | Where it lives in C++ | Plan in C# |
|-----|----------------------|------------|
| Outbound send queue + backpressure | `m_qSEND`, `SendQueueFull`, `Post` | Same `PacketSession` (use `Channel<>` or `PipeWriter` flow control). |
| IOCP completion thread pool | `WSAStartup`, `CreateIoCompletionPort` | Not needed — `async/await` + .NET ThreadPool. |
| Buffer pool / `ArrayPool` reuse | `CPacket::ExpandIoBuffer`, `Flush` | Use `System.Buffers.ArrayPool<byte>.Shared` inside `PacketSession`. |
| Per-session critical section | `CRITICAL_SECTION m_cs` everywhere | `PacketCodec` is intentionally NOT thread-safe. Caller (`PacketSession`) serializes via single async loop per session. Document on `PacketCodec`. |
| `Listen` / `Connect` / `ConnectEx` socket setup | `Session.cpp:174-612` | `TcpListener` / `Socket.ConnectAsync` in higher layer (`FourStory.Login` / `.Map`). |
| Connect-side TLS / cert handling | Not in legacy | Out of scope for protocol-compatibility phase. |

None of these belong inside `FourStory.Protocol` itself — they're transport/glue concerns the next layer up handles. Their absence does NOT block Phase 2 protocol verification.

---

## 5. 🟢 Coverage extras present in C# but not in C++

- `MessageId.g.cs` — typed enum for all 1542 IDs, generated from CSV. C++ had `#define` macros only.
- `Encoding/Cp1252.cs` — central encoding instance with provider auto-registration. C++ relied on system MBCS locale.
- `PacketDecryptResult` enum distinguishes `Ok` / `SequenceMismatch` / `ChecksumMismatch`. C++ collapsed both into `BOOL FALSE`.
- Strong-typed `PeerType` enum. C++ used `BYTE m_bSessionType` with `SESSION_SERVER` / `SESSION_CLIENT` magic numbers.

---

## 6. Test coverage matrix

| Behavior | Tested? | Test file |
|----------|---------|-----------|
| Primitive round-trip (all 9 scalar types + string + blob) | ✅ | `PacketReaderWriterTests.PrimitiveRoundTrip_PreservesValues` |
| String wire format (LE int32 length + CP1252 bytes, no NUL) | ✅ | `PacketReaderWriterTests.String_IsLittleEndianLengthPrefixed_Cp1252Bytes` |
| Reader underflow throws | ✅ | `PacketReaderWriterTests.Reader_UnderflowThrows` |
| XOR body encrypt/decrypt round-trip (full INT64 chunks) | ✅ | `XorLayerTests.EncryptBody_DecryptBody_RoundTrip_RestoresPlaintext` |
| XOR body with partial tail bytes (the tricky rolling-CRC branch) | ✅ | `XorLayerTests.EncryptBody_DecryptBody_HandlesPartialTailBytes` |
| Tampered body fails checksum | ✅ | `XorLayerTests.DecryptBody_TamperedBody_FailsChecksum` |
| Header obfuscation symmetry (wSize stays plaintext) | ✅ | `XorLayerTests.EncryptHeader_DecryptHeader_RoundTrip_RestoresHeader` |
| Key table modulo-7 selection | ✅ | `XorLayerTests.KeyTable_KeyFor_MatchesModulo7` |
| RC4 against RFC 6229 test vectors | ✅ | `Rc4LayerTests.Rc4_MatchesRfc6229_FirstBytes` |
| RC4 symmetry | ✅ | `Rc4LayerTests.Rc4_IsSymmetric` |
| `RawSecretKey` layout (39 bytes, 0x92/0x94 at expected offsets) | ✅ | `Rc4LayerTests.RawSecretKey_Is39Bytes_MatchesC44StoryLayout` |
| End-to-end client → server decrypt (XOR+RC4) | ✅ (post-fix) | `PacketCodecTests.ServerCodec_DecryptsClientCraftedPacket_WithSequenceAndChecksum` |
| Out-of-order sequence rejected | ✅ | `PacketCodecTests.ServerCodec_RejectsOutOfOrderSequence` |
| Tampered body in full pipeline rejected | ✅ | `PacketCodecTests.ServerCodec_RejectsTamperedBody` |
| Outbound encrypt is XOR-only (no RC4) | ✅ | `PacketCodecTests.ServerCodec_RoundTrip_OutboundDecodedByMirrorCodec` |
| Plaintext mode bypasses crypto | ✅ | `PacketCodecTests.Plaintext_WhenCryptDisabled_PacketUnchanged` |
| wSize validation on encrypt | ✅ | `PacketCodecTests.Encrypt_WithMismatchedWSize_Throws` |

### Tests NOT yet present (low priority)
- Real captured packet decryption (need legacy binary capture).
- Multi-packet sequence (packet 1, 2, 3 in a row with keys rotating through table).
- Concurrent codec usage (should fail loudly; or guarded by Pipeline serialization).
- Zero-length body (header-only packet).

---

## 7. Recommendations

### Fix now (this commit)
1. ✅ RC4 offset bug — fixed.

### Defer to integration testing
2. Verify RC4 key is 128-bit (not 40-bit export-grade) by decrypting a real client packet.
3. Verify exact bytes of `g_strSecretKey` via hex dump of legacy `Session.obj` or live binary.

### Defer to next sub-tasks of Phase 1
4. ✅ `PacketSession` implemented under `FourStory.Protocol.Session.PacketSession`.
5. Decide thread-safety policy (single-loop per session = caller serializes → safe).
6. Add multi-packet sequence test now that `PacketSession` exists.

### Defer to Phase 2 (TLoginSvr port)
7. Find the activation point of `m_bUseCrypt = true` (PROTOCOL.md §5 Q4).
8. Wire up actual client connection and validate end-to-end against the leaked binary, if available.

---

## 8. 2026-05-17 Login/Map audit fixes

Cross-checked the C# rewrite against the C++ source for three suspected protocol-flow divergences. All three confirmed and fixed in this pass.

### 8.1 ✅ `CS_CREATECHAR_ACK` was missing the trailing `bLevel` byte
- C++ reference: `Server/TLoginSvr/CSSender.cpp:68-103` — `SendCreateCharAck` takes both `BYTE bCreateCnt` and `BYTE bLevel` and writes both as the final trailing bytes.
- C# bug: `LobbyHandlers.OnCreateCharAsync` (lines 212-231) only wrote `RemainingSlots` (= `bCreateCnt`). The buffer size constant was 12 instead of 13.
- Fix: `CreateCharOutcome` now carries the new char's `Level`; `LobbyHandlers` appends `w.WriteByte(outcome.Level)` and the buffer size is `1+4+(4+N)+13`. (`src/FourStory.Login/Auth/CharService.cs`, `src/FourStory.Login/Handlers/LobbyHandlers.cs`)

### 8.2 ✅ `MapServerLocator` was not filtering on `bType`
- C++ reference: `Server/TLoginSvr/CSHandler.cpp:1400-1406` calls the `TRoute` SP with `m_bType = SVRGRP_MAPSVR` (= 4, from `Lib/Own/TProtocol/include/CTProtocol.h:30`).
- C# bug: the previous "best-effort" lookup took the first active row for the group regardless of role, so a non-map row in `TSERVER` would mis-route the client.
- Fix: added `.Where(s => s.bType == (byte)ServerType.MapSvr)` to the query, and defined `ServerType.MapSvr = 4` in `FourStory.Shared`. (`src/FourStory.Shared/ServerType.cs`, `src/FourStory.Login/Services/MapServerLocator.cs`)

### 8.3 ✅ `Map/ConnectHandler.ValidateAsync` was only stamping a subset of `TCURRENTUSER`
- C++ reference: `Server/TMapSvr/DBAccess.h:5076-5103` — the `TEnterServer` SP stamps `dwKEY`, `dwUserID`, `dwCharID`, `bGroupID`, `bChannel`, `szIPAddr`, `wPort`.
- C# bug: only `dwCharID`, `bChannel`, `dEnterDate` were stamped. `szIPAddr`, `wPort`, `bGroupID` were left untouched, so downstream routing would see stale (login-side) values.
- Fix: introduced `MapServerInfo` (singleton, configured via `Map:GroupId` in `appsettings.json`); `MapConnection` now captures the accepted socket's `LocalEndPoint` (the IP/port the client connected to); `ConnectHandler` stamps `bGroupID = serverInfo.GroupId`, `szIPAddr = conn.LocalAddress`, `wPort = conn.LocalPort` in addition to the existing fields. (`src/FourStory.Map/MapServerInfo.cs`, `src/FourStory.Map/Network/MapServer.cs`, `src/FourStory.Map/Network/MapConnection.cs`, `src/FourStory.Map/Handlers/ConnectHandler.cs`, `src/FourStory.Map/Program.cs`)

---

## 12. World ↔ Map character bootstrap — first end-to-end pass

The handshake World ↔ Map handshake was the previous milestone (MW_CONNECT_ACK + MW_ADDCHAR_ACK). This session closes the next leg: when a client passes `CS_CONNECT_REQ`, Map → World → Map → MapSessionState now flows end-to-end. `InitMap` (the per-character SC fan-out) is still stubbed because nothing reads `MapSessionState.MapId` etc. yet.

### 12.1 Pipeline now wired

```
Client → Map  : CS_CONNECT_REQ
Map           : ValidateAsync  → stamp TCURRENTUSER → register in MapConnectionRegistry
Map → World   : MW_ADDCHAR_ACK (0x9003)  {charId, key, ipAddr, port, userId}
World         : EF lookup TCHARTABLE by charId
World → Map   : MW_ENTERSVR_REQ (0x9008) {bDBLoad=0, charId, key, snapshot…}
Map           : EnterSvrHandler → MapSessionState populated → HasSnapshot=true
Client → Map  : CS_CONREADY_REQ → IsReady=true
Map           : if (IsReady && HasSnapshot) → [stub] InitMap trigger
```

### 12.2 Architectural shortcut vs. C++

The legacy server bounces character load through DM:

```
World → Map: MW_ENTERSVR_REQ {bDBLoad=1, charId, key}       (simple form)
Map → DM   : DM_ENTERMAPSVR_REQ
DM  → Map  : DM_ENTERMAPSVR_ACK (validation result)
Map → DM   : DM_LOADCHAR_REQ
DM  → Map  : DM_LOADCHAR_ACK   (full TCHARACTER snapshot)
```

We have no DM peer; World owns EF access to `GameDbContext` directly. So we collapse the chain — World loads `TCHARTABLE` and ships the snapshot inline as `MW_ENTERSVR_REQ` with `bDBLoad=0`. The legacy C++ Map already supports this branch (`SSHandler.cpp:3090-3094` copies the inline packet into `pPlayer->m_mainchar` when `bDBLoad==FALSE`), but in C++ that branch is only used for the temp-server channel-hop path; here it's the only path. Wire format from `bDBLoad` onward therefore deliberately diverges from legacy MW_ENTERSVR_REQ. See `FourStory.World.Handlers.EnterSvrPacket` for the byte layout.

### 12.3 Files added / changed

| File | Change |
|------|--------|
| `src/FourStory.Map/Network/MapConnectionRegistry.cs` | **NEW.** Thread-safe `charId → MapConnection` lookup, registered as singleton. Mirrors C++ `m_mapPlayer` / `FindPlayer` (Server/TMapSvr/SSHandler.cpp:3083). |
| `src/FourStory.Map/Network/MapConnection.cs` | Added `AssociateCharacter(charId)` + `DisposeAsync` unregister hook. Extended `MapSessionState` with 36 char-snapshot fields + `HasSnapshot` flag mirroring `CTPlayer` fields filled by `OnDM_LOADCHAR_ACK` (SSHandler.cpp:4484-4629). |
| `src/FourStory.Map/Network/MapServer.cs` | Resolves and threads `MapConnectionRegistry` through every accepted `MapConnection`. |
| `src/FourStory.Map/Handlers/ConnectHandler.cs` | On `CS_CONNECT_REQ` success, calls `conn.AssociateCharacter(charId)` so World→Map traffic can find the session. |
| `src/FourStory.Map/Handlers/EnterSvrHandler.cs` | **NEW.** Handles `MW_ENTERSVR_REQ` (0x9008) on the WorldClient side. Parses 36-field snapshot, validates key against session, populates `MapSessionState`. Fires the (still stubbed) InitMap trigger if `IsReady` was already set. |
| `src/FourStory.Map/Handlers/ReadyHandler.cs` | Symmetric InitMap-trigger branch added — when `CS_CONREADY_REQ` arrives after the snapshot. |
| `src/FourStory.Map/Program.cs` | Registers `MapConnectionRegistry` singleton + wires `EnterSvrHandler` into `WorldClientDispatcher`. |
| `src/FourStory.World/Handlers/MapAddCharHandler.cs` | **Replaced stub.** Now loads `TCHARTABLE` (rejects missing/soft-deleted), builds `MW_ENTERSVR_REQ` payload via new internal `EnterSvrPacket` helper, sends back over the same `WorldConnection.Session`. |

### 12.4 What `MapSessionState` looks like after the bootstrap

Populated:
- `UserId`, `CharId`, `Key`, `Channel`, `IpAddr`, `Port` (from `CS_CONNECT_REQ`)
- `Name`, `StartAct`, `RealSex`, `Class`, `Level`, `Race`, `Country`, `OriCountry`, `Sex`, `Hair`, `Face`, `Body`, `Pants`, `Hand`, `Foot`, `HelmetHide`, `Gold`, `Silver`, `Cooper`, `Exp`, `HP`, `MP`, `SkillPoint`, `Region`, `GuildLeave`, `GuildLeaveTime`, `MapId`, `SpawnId`, `LastSpawnId`, `LastDestination`, `TemptedMon`, `Aftermath`, `PosX/Y/Z`, `Dir`, `StatLevel`, `StatPoint`, `StatExp`, `RankPoint` (from `MW_ENTERSVR_REQ` snapshot)
- `IsReady` (from `CS_CONREADY_REQ`)
- `HasSnapshot` = `true`

NOT populated (deferred until matching subsystems land):
- Inventory, skills, quests, companions, guild bindings, party bindings, tactics, mounts, titles, monthly-rank — these live in separate SS subsystems on the C++ side (`MW_CHARDATA_*`, `DM_LOADCHAR_ACK`'s table-by-table reads at `SSHandler.cpp:4200-4400`).
- TSECURE (secure-code), PCBANG state, post counts, lucky number — fields exist in legacy `DM_LOADCHAR_ACK` but require subsystems we haven't ported.

### 12.5 Open TODOs surfaced

- **`InitMap` fan-out** (next milestone). Read `MapSessionState.MapId/PosX/Y/Z/Country` and send `CS_ADDCONNECT_ACK` → `CS_CHARINFO_ACK` → `CS_ENTER_ACK` (×N for nearby players) → inventory/skills/quest bundles → `CS_SENDSADDLE_REQ`. C++ ref: `TMapSvr.cpp:7909-8230` + `SSHandler.cpp:6128`.
- **Negative-result `MW_ENTERSVR_ACK`** for missing/deleted char or duplicate session — legacy code returns `CN_INTERNAL` / `CN_ALREADYEXIST` and Map sends `MW_ENTERSVR_ACK(bResult≠0)`. Currently we drop on World side; the client will hang until timeout.
- **In-memory `TCHARACTER` cache** on World — legacy code reuses `m_mapTCHAR` for channel hops to avoid DB hits (SSHandler.cpp:727-845). Not needed for first connect; needed for channel switching.
- **World re-entry validation** — second `MW_ADDCHAR_ACK` for an already-tracked charId must check `(key, ipAddr, port)` tuple and send `MW_INVALIDCHAR_REQ` on mismatch (SSHandler.cpp:803-826).
- **Party / guild / tactics rehydration** on the World side after EF load. Legacy `OnMW_CHARDATA_REQ` re-attaches `m_pParty`, `m_pGuild`, `m_pTactics` from in-memory dicts. Skipped until those subsystems land.

---

## 9. 2026-05-17 Map server: CS_CONREADY_REQ stub

Step forward on the Map server: the post-validate handshake packet (`CS_CONREADY_REQ`, 0x5288) is now accepted and validated, even though the full `InitMap` flow it gates in C++ is intentionally deferred.

### 9.1 ✅ `Map/ReadyHandler` — accepts the empty "I'm ready" packet
- C++ reference: `Server/TMapSvr/CSHandler.cpp:402-415` — `OnCS_CONREADY_REQ` consumes an empty-payload packet; depending on per-player state it calls either `InitMap(pPlayer)` (first entry) or `pPlayer->m_pMAP->EnterMAP(pPlayer, FALSE)` (re-entry).
- `InitMap` itself (`Server/TMapSvr/TMapSvr.cpp:7909-8230`) is hundreds of lines: resolves `CTMap` for `(channel, partyId, mapId)`, handles castle / sky-garden / tournament special cases, repositions companions/pets, then calls `EnterMAP(pPlayer, TRUE)` which fans out dozens of SC packets to populate the client's world view (`CS_ADDCONNECT_ACK`, `CS_CHARINFO_ACK`, `CS_ENTER_ACK` per neighbour, inventory, skills, quests, ...).
- Implementation in C#: `ReadyHandler` (new file, `src/FourStory.Map/Handlers/ReadyHandler.cs`) — registers for `MessageId.CS_CONREADY_REQ`, asserts the empty payload, verifies the session has passed `ConnectHandler.ValidateAsync` (i.e., `MapSessionState.IsAuthenticated`), and sets a new `MapSessionState.IsReady` flag. Does NOT call `InitMap` yet.
- Why not call `InitMap` yet: `CTPlayer::m_wMapID`, `m_fPosX/Y/Z`, `m_bChannel`, etc. are populated server-side by SS (server-to-server) messages from TWorldSvr in response to the `MW_ADDCHAR_ACK` the map fires inside `OnCS_CONNECT_REQ` (`Server/TMapSvr/SSSender.cpp:237`; write-sites for the snapshot fields live at `Server/TMapSvr/SSHandler.cpp:4653`, `5714`, `13266`). Until TWorldSvr is ported and SS handlers exist in the C# side, there is no character snapshot to feed `InitMap`. Sending a partial `CS_ENTER_ACK`/`CS_CHARINFO_ACK` now would mislead the (eventual) client.
- Wire response: none — matches the C++ behavior, which returns `EC_NOERROR` without sending anything synchronously (all outgoing packets are produced inside `EnterMAP`).
- Plumbing: `MapSessionState` gained a `bool IsReady` flag; `Program.cs` instantiates and registers `ReadyHandler` alongside the existing `ConnectHandler`.

### 9.2 Remaining work for a complete "player in world" flow
What's still needed before a real client could enter the world end-to-end after `CS_CONREADY_REQ`:
1. **TWorldSvr port** — at minimum the SS dispatch shell so the map can publish `MW_ADDCHAR_ACK` and receive back the character snapshot.
2. **SS handlers on the map side** — `OnMS_ADDCHAR_ACK` / `OnMS_TELEPORT_ACK` etc. (see `Server/TMapSvr/SSHandler.cpp:4640-4760` for the canonical entry path) to populate `MapSessionState` with `MapId`, `PosX/Y/Z`, `Country`, `SpawnId`, party id, etc.
3. **`InitMap` port** — `CTMapSvrModule::InitMap` (`TMapSvr.cpp:7909`) → `CTMap::EnterMAP` (`TMap.cpp` — `EnterMAP` body) → the cell/AOI broadcast layer.
4. **The fanned-out SC packets** — character base info, inventory, skills, quests, neighbours; each maps to its own handler in `CSSender.cpp`.

Tracking issue: tied into Phase 3 (TWorldSvr) — the Map server can't progress past this stub on its own.

---

## 10. 2026-05-17 TWorldSvr scaffold

Spun up the C# `FourStory.World` project so the Map server's `CS_CONREADY_REQ` flow has somewhere to plug its eventual `MW_ADDCHAR_REQ` into. This pass is scaffold-only — the SS dispatch shell, the TCP accept loop, and a single handshake handler.

### 10.1 ✅ New project: `FourStory.World`
- Layout mirrors `FourStory.Map`: SDK-style `Microsoft.NET.Sdk.Worker` csproj, same package set (`Microsoft.Extensions.Hosting`, `Serilog.Extensions.Hosting`, `Serilog.Sinks.Console`), same project references (`FourStory.Protocol`, `FourStory.Shared`, `FourStory.Persistence`).
- Files: `Program.cs` (Generic Host bootstrap), `WorldServerInfo.cs` (record: `GroupId`, `ServerId`, `Port`), `Network/WorldServer.cs` (TCP accept loop, `BackgroundService`), `Network/WorldConnection.cs` (per-peer session + `WorldPeerState` + `WorldPacketDispatcher`), `Handlers/MapConnectHandler.cs` (one stub handler), `appsettings.json` / `appsettings.Development.json` / `Properties/launchSettings.json`.
- Added to `src/FourStory.slnx` so `dotnet build` at solution root picks it up.
- Default port: `3815` (`ProtocolConstants.DefaultWorldPort`, matches C++ `DEF_WORLDPORT` in `Lib/Own/TProtocol/include/CTProtocol.h`).
- Entry-point template was `Server/TWorldSvr/TWorldSvr.cpp::_tWinMain` → `CTWorldSvrModule::InitInstance` (line 195) → `InitNetwork` (line 487). The C# Generic Host replaces the WinMain → ATL service shell; the `BackgroundService` accept loop replaces the IOCP `AcceptEx` pump (line 531).
- Peer-side codec is `PeerType.Server` (no RC4) — World only services other server processes (Map / Login / DM / Manager), never end-user clients.

### 10.2 ✅ Ported handler: `MW_CONNECT_ACK` (0x9002)
- C++ reference: `Server/TWorldSvr/SSHandler.cpp:592-702` (`CTWorldSvrModule::OnMW_CONNECT_ACK`).
- Wire payload: `WORD wServerID`, `BYTE bCount`, then `bCount × BYTE bChannel`. Despite the `_ACK` suffix this is the *first* packet flowing from a connecting Map peer — legacy naming reflects the World process's request-side role in the cluster lifecycle, not the TCP direction.
- C# behaviour: parse payload, reject duplicate-srv-id reconnect with a warning (C++ returns `EC_SESSION_DUPSERVERID`; a follow-up will close the session), populate `WorldPeerState.ServerId` and `WorldPeerState.Channels`, log. The bootstrap-state fan-out (`SendMW_LOCALENABLE_REQ`, `SendMW_MISSIONENABLE_REQ`, `SendMW_EVENTUPDATE_REQ`, `SendMW_CASHITEMSALE_REQ`, `SendMW_CASTLEAPPLICANTCOUNT_REQ`, `SendMW_MONTHRANKLIST_REQ`, `TournamentInfo`) is deferred — none of those subsystems exist in the C# side yet, and the per-character snapshot flow (`MW_ADDCHAR_ACK` → `MW_ENTERSVR_REQ`) doesn't depend on it.

### 10.3 Remaining work for functional Map ↔ World communication
To get a real client past `CS_CONREADY_REQ` into `InitMap`, the next pieces needed:
1. **Map-side outgoing client**: a `WorldClient` in `FourStory.Map` that opens a long-lived TCP connection to World on startup, sends `MW_CONNECT_ACK` with its server id + channels, and pipes per-player SS calls through it. (C++ parallel: `CTMapSvrModule::m_pSESSION` on the Map side.)
2. **`MW_ADDCHAR_REQ` from Map → World**: fired inside `Map/ConnectHandler.OnConnectAsync` right after a successful `CS_CONNECT_REQ` validation. C++: `Server/TMapSvr/SSSender.cpp::SendMW_ADDCHAR_REQ`.
3. **`OnMW_ADDCHAR_ACK` on World**: load `TCHARACTER` from `GameDbContext`, send `MW_ENTERSVR_REQ` back. C++: `Server/TWorldSvr/SSHandler.cpp:704-845`.
4. **`OnMW_ENTERSVR_ACK` on Map**: populates `MapSessionState` with `MapId`, `PosX/Y/Z`, `Channel`, party id — the snapshot data `ReadyHandler` is currently waiting for. C++: `Server/TMapSvr/SSHandler.cpp:4640-4760`.
5. **`InitMap` port on Map**: once the snapshot lands, `ReadyHandler` can call it. See §9.2.

### 10.4 Notes / unclear bits
- The C++ World server stores DB credentials in registry (`HKLM\…\Services\<svc>\Config`); the C# port uses standard `appsettings.json` `ConnectionStrings`. Equivalent but not byte-for-byte.
- The original World process also runs `BatchThread`, `DBThread`, `TimerThread`, and N `WorkThread`s for IOCP work (`Server/TWorldSvr/TWorldSvr.cpp::CreateThreads`, line 398). The C# port relies on the .NET thread pool + async/await; we'll add explicit background services if any of those threads turn out to do work that doesn't translate (e.g. the timer thread schedules battle/tournament events).
- `DBAccess.cpp` was not explored in this pass — it's the next thing to dissect when we start porting actual gameplay queries onto World.
- Sandbox has no `dotnet` toolchain, so build was not run; verified manually that namespaces, project references, and the `MessageId.MW_CONNECT_ACK = 0x9002` enum entry all exist and that signatures of `PacketCodec(PeerType)`, `PacketSession(Stream, PacketCodec, bool)`, `PacketSession.RunAsync(PacketHandler, CT)` match the Map server's usage.

## 11. 2026-05-17 Map → World outbound SS link

Closed the Map-side hole identified in §10.3: a Map process now opens an outbound SS connection to World on startup, sends the handshake, and forwards each connecting character to World via `MW_ADDCHAR_ACK`.

### 11.1 ✅ New: `FourStory.Map.Network.WorldClient`
- `BackgroundService` that owns one long-lived TCP connection to `WorldEndpoint` (config: `World:Host` / `World:Port`, defaults to `127.0.0.1:3815`).
- Reuses `PacketSession` + `PacketCodec(PeerType.Server)` — no RC4 between Map and World. Matches C++ `m_world.m_bUseCrypt = FALSE` at `Server/TMapSvr/TMapSvr.cpp:947`.
- Capped exponential backoff (1s → 30s) on connect failure / session drop. Legacy aborts the process on World connect failure (`EC_INITSERVICE_CONNECTWORLD` at `TMapSvr.cpp:950`); the C# port stays alive and retries so Map can ride out a temporary World blip.
- Inbound SS dispatch goes through a new `WorldClientDispatcher` (symmetric to `MapPacketDispatcher` / `WorldPacketDispatcher`). Empty out of the gate — handlers for `MW_ENTERSVR_REQ`, `MW_CHARDATA_REQ`, `MW_INVALIDCHAR_REQ`, etc. will register here as they land.
- Concurrency: `_state` is an immutable `(Session?, ReadyTcs)` record swapped atomically with `Interlocked.Exchange`. Callers waiting on a not-yet-connected session block on `ReadyTcs.Task` with a 2-second timeout, then either return the live session or throw.

### 11.2 ✅ Ported sender: `MW_CONNECT_ACK` (0x9002) Map → World
- Fired immediately after TCP connect, before signalling "ready" to handler callers (so they never observe a half-handshaked session).
- C++ reference: `Server/TMapSvr/SSSender.cpp:210-223` (`CTMapSvrModule::SendMW_CONNECT_ACK`).
- Wire payload, byte-for-byte:
  - `WORD wServerID = MAKEWORD(m_bServerID, SVRGRP_MAPSVR)` — low byte = `Map:ServerId`, high byte = `4` (`SVRGRP_MAPSVR` from `Lib/Own/TProtocol/include/CTProtocol.h:30`).
  - `BYTE bCount` — number of channels.
  - `BYTE bChannel × bCount` — from `Map:Channels` config (default `[1]`). Static config for now; once the channel data layer lands this will move into a `ChannelService` that mirrors `m_mapTLOGCHANNEL` from `DBAccess.cpp`.

### 11.3 ✅ Ported sender: `MW_ADDCHAR_ACK` (0x9003) Map → World
- Fired from `ConnectHandler.OnConnectAsync` after a successful `ValidateAsync`, right after writing back `CS_CONNECT_ACK` and flipping the codec to encrypted. Matches the call at `Server/TMapSvr/CSHandler.cpp:388-393`.
- C++ reference: `Server/TMapSvr/SSSender.cpp:237-253` (`CTMapSvrModule::SendMW_ADDCHAR_ACK`).
- Wire payload, in order: `DWORD dwCharID`, `DWORD dwKEY`, `DWORD dwIPAddr`, `WORD wPort`, `DWORD dwUserID`.
- `dwIPAddr` / `wPort` are the Map endpoint the client claims (TLoginSvr → client → Map relay via CS_CONNECT_REQ). Stored on `MapSessionState` (new `IpAddr` / `Port` fields) so any later sender can reuse them.
- Send is awaited (so codec state doesn't race), but errors are caught and logged — the C++ flow doesn't synchronously wait on World either; the response chain (`MW_ENTERSVR_REQ` → DM → `MW_CHARDATA_ACK`) is asynchronous.

### 11.4 ✅ Ported handler: `MW_ADDCHAR_ACK` on World (stub)
- New `FourStory.World.Handlers.MapAddCharHandler` registered in `WorldPacketDispatcher`.
- C++ reference: `Server/TWorldSvr/SSHandler.cpp:704-845` (`CTWorldSvrModule::OnMW_ADDCHAR_ACK`).
- Current scope: parse payload, guard against arrivals before `MW_CONNECT_ACK`, log. **No `TCHARACTER` allocation, no `MW_ENTERSVR_REQ` send-back yet.** Reason: the character-store + DB-load chain (`MW_ENTERSVR_REQ` → DM → `MW_CHARDATA_ACK` → fan-out to Map) has no C# plumbing — no `TCHARACTER` service, no DM peer, no Map-side handler for the response. Sending a half-baked `MW_ENTERSVR_REQ` would leave the Map peer waiting on DB results that never arrive. Better to log + stop than to provoke a hang.

### 11.5 ⚠️ Renamed in the porting plan: `MW_ADDCHAR_REQ` → `MW_ADDCHAR_ACK`
§10.3 listed the next-step packet as `MW_ADDCHAR_REQ`. The actual C++ packet name (and the only one defined in `Lib/Own/TProtocol/include/MWProtocol.h:20` + `MessageId.g.cs:1038`) is `MW_ADDCHAR_ACK = 0x9003`. There is no `_REQ` variant. The legacy `_ACK` suffix here reflects World's request-side role in the cluster lifecycle, not the TCP/SS direction — same convention as `MW_CONNECT_ACK` (§10.2). Followed the actual wire name in this commit.

### 11.6 Remaining work for "client appears in map"
The chain is still incomplete — `CS_CONREADY_REQ` arrives at Map but `InitMap` has no character snapshot to seed it with. Outstanding from §10.3:
1. ✅ **Map-side outgoing client** (`WorldClient`) — done in this pass.
2. ✅ **`MW_ADDCHAR_ACK` from Map → World** — done.
3. **`OnMW_ADDCHAR_ACK` on World** — *stub only*, full impl deferred (§11.4). Needs: in-memory `TCHARACTER` store, DM peer, `MW_ENTERSVR_REQ` sender, and the DB load chain to be ported from `DBAccess.cpp` / `SSHandler.cpp:727-841`.
4. **`OnMW_ENTERSVR_REQ` on Map** — not yet started. Needs a register entry in `WorldClientDispatcher` that pulls `TCHARACTER` from DM and populates `MapSessionState.MapId/PosX/Y/Z/Channel/PartyId`. C++: `Server/TMapSvr/SSHandler.cpp:4640-4760`.
5. **`InitMap` port on Map** — see §9.2.

### 11.7 Notes / unclear bits
- `Map:Channels` is static config (defaults to `[1]`). The legacy server enumerates `TCHANNEL` rows out of `TGLOBAL` (`DBAccess.cpp`), then drops the channels it cannot host. Until a channel service lands, multi-channel Map deployments need to set `Map:Channels` by hand in `appsettings.json`.
- `Map:ServerId` is new — added to `MapServerInfo` and pushed into the `MW_CONNECT_ACK` payload. Defaults to `1`; must be unique per Map process within a World group (C++ enforces this via `EC_SESSION_DUPSERVERID` in `OnMW_CONNECT_ACK`).
- The `WorldClient` lifecycle assumes World is reachable at startup. If it isn't, `CS_CONNECT_REQ` handlers will return successful `CS_CONNECT_ACK` to the client (because the DB validation passes) but the `SendAddCharAckAsync` call times out after 2s and is logged as an error. The client will then hang on `CS_CONREADY_REQ` — same end-user effect as in the C++ server when World is down. No special handling here; the operator-facing fix is "bring World up before Map".
- Sandbox has no `dotnet` toolchain, so build was not run. Verified manually:
  - `MessageId.MW_CONNECT_ACK = 0x9002` and `MessageId.MW_ADDCHAR_ACK = 0x9003` exist in `MessageId.g.cs`.
  - `PacketWriter.WriteUInt16/UInt32/Byte` cover the wire layout.
  - `PacketSession.SendAsync(MessageId, ReadOnlySpan<byte>, CT)` signature matches the WorldClient send call.
  - `ConnectHandler` constructor now resolves through `ActivatorUtilities.CreateInstance` against the DI graph (`WorldClient` is registered as singleton + hosted service via the same instance).
  - `MapServerInfo` is only used in Map (no other project references it after the `ServerId` field was added).
