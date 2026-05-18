# Legacy TClient build — feasibility & requirements

Status: **Not yet built.** Survey done 2026-05-17.

## What `Client/TClient.sln` is

Visual Studio 2017 solution containing 6 projects:
- **TClient** (main exe — DirectX 9 + MFC client, ~584 .cpp/.h files)
- **Engine Lib** (DirectX renderer + audio + crypt + Rijndael + packet, ~46k LOC)
- **TChart** (loads `T*CHART` reference data files, ~15k LOC)
- **TCML** (lex/yacc parser, ~5k LOC)
- **TComp** (GDI/MFC UI components, ~13k LOC)
- **HwidLib** (hardware ID collection, ~1.5k LOC)

Plus `TachyonControl` (~50k LOC of MFC controls) referenced indirectly.

## Build requirements (from `TClient.vcxproj`)

| Setting | Value | Note |
|---------|-------|------|
| Platform toolset | `v141` | VS 2017 C++ build tools. Retargetable to v143 (VS 2022) with project conversion. |
| Windows SDK | `10.0.17763.0` | Windows 10 1809 SDK. Retargetable to newer. |
| MFC | `Static` | Requires "C++ MFC for v143 build tools" component. |
| Character set | `MultiByte` | Confirms CP1252 assumption from PROTOCOL.md. |
| ATL | disabled | |
| Configs | Debug/Release/Test Release × Win32/x64 | |

## Current state on this machine

- ❌ **Visual Studio not installed** (no `vswhere` results, no `MSBuild.exe` found).
- ❌ **No precompiled `TClient.exe`** anywhere in the source tree.
- ✅ **DirectX 9 SDK (June 2010)** present at `Lib/3rdParty/DirectX9 (June 2010)/`.
- ✅ **HShield / XTrap / Apex / NPGame** headers + libs present (legacy anti-cheat).
- ✅ **zlib + openssl** headers + libs present.

## Paths forward

Roughly in order of effort/value:

### Path 1 — Use existing `MockClient` (zero effort, partial value)

What we have now: `FourStory.Login.IntegrationTests.MockClient` speaks the wire protocol from the client perspective. The 38 passing tests prove:
- Wire framing (16B header) round-trips
- XOR + RC4 crypto round-trips on real TCP
- Login + lobby + char-create + char-delete flows work end-to-end

What it can't tell us:
- Whether `g_strSecretKey` bytes match exactly what a real client would send (Q2)
- Whether RC4 key is 128-bit MD5 hash (Q6)
- Whether `m_bUseCrypt` flips at the exact packet boundary we assumed (Q4)

Because the mock client **uses the same protocol implementation** the server does, it cannot detect a wrong-but-self-consistent decision in that protocol.

### Path 2 — Build TClient.exe locally

Requirements:
1. Install **Visual Studio 2022** (Community is free) with workloads:
   - "Desktop development with C++"
   - Individual components: "C++ MFC for v143 build tools (x86 & x64)", "C++ ATL for v143 build tools (x86 & x64)"
   - Individual components: "MSVC v141 - VS 2017 C++ x64/x86 build tools" *(skip if retargeting)*
   - "Windows 10 SDK (10.0.17763.0)" *(or retarget to newer)*

   Total install: ~10 GB.

2. Open `Client/TClient.sln`:
   - VS may prompt to retarget — accept (v141 → v143).
   - Resolve any post-retarget warnings.

3. Likely build issues:
   - DirectX 9 SDK headers conflict with newer Windows SDK `<dsound.h>` etc. Project sets correct include order via `AdditionalIncludeDirectories` but may need tweaking.
   - HShield/XTrap libs are link-time deps. They're present but may have v141-specific CRT.
   - `Apex/Client` headers — proprietary; may include checks that fail on modern Windows.

4. Run `TClient.exe` against `127.0.0.1:4815` (our new login server). Watch protocol behavior; capture a CS_LOGIN_REQ packet via Wireshark on loopback to confirm bytes.

**Effort: half a day to a day** if no major linker issues. Could be longer if anti-cheat libs reject modern Windows.

### Path 3 — Find a redistributed 4Story client binary

4Story was a public game; community servers (RaGEZONE etc.) maintain client binaries. If you have a working private-server client from the same release era as this leak, it should speak the same wire protocol. Drop the EXE into a folder and point it at `127.0.0.1:4815`. **Zero compile effort, fast feedback.** Caveat: licensing / redistribution rules.

### Path 4 — Skip until needed

Keep MockClient as the validation harness. Only invest in path 2 or 3 if a wire-format ambiguity actually blocks a real player connecting. We have an exhaustive understanding of the protocol from the C++ source; the only ambiguities are documented as PROTOCOL.md §5 Q2/Q4/Q6, none of which block forward progress on server-side handlers.

## Recommendation

**Path 4 for now, path 3 when ready to onboard a real player.** Path 2 is overkill until we hit a wire-format mismatch we can't resolve from the source.

If the user wants to do path 2, this document is the spec.

---

## Update 2026-05-17 evening — Path 2 succeeded

The user installed Visual Studio Community 2026 (v18.6) with C++ MFC. TClient.sln **builds end-to-end** producing `C:\Users\tomas\Desktop\Sources 5.0 (Araz)\Sources 5.0 (Araz)\4STORY_PW CLIENT 01\TClient.exe` (5.63 MB, debug symbols included).

### Build invocation
```pwsh
$msbuild = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe'
$sln     = 'C:\Users\tomas\Desktop\Sources 5.0 (Araz)\Sources 5.0 (Araz)\4Story_5.0_Source\Client\TClient.sln'
& $msbuild $sln `
    '/p:Configuration=Release' '/p:Platform=x86' `
    '/p:PlatformToolset=v145' `
    '/p:WindowsTargetPlatformVersion=10.0' `
    /m /verbosity:minimal /nologo
```

### Required VS 2026 components
- **NativeDesktop workload** (already installed by default with C++ tools)
- **"C++ MFC for latest v143/v145 build tools (x86 & x64)"** — `Microsoft.VisualStudio.Component.VC.ATLMFC` only installs ATL. MFC needs the separate "MFC for v143 build tools" component, installed via VS Installer GUI.
- **Windows 11 SDK 10.0.26100** (in lieu of the original v141/Win10 SDK 17763).

The `/p:PlatformToolset=v145` retarget works at the command line — no need to modify the .vcxproj files.

### Source patches applied for VS 2026 (C++20 default) compatibility

#### 1. `std::binary_function` removed in C++17
7 occurrences across 5 files used `: public binary_function<A, B, R>` for type-traits. Pattern-replaced via PowerShell regex (removed the inheritance — the structs use only `operator()` which doesn't need it):

```pwsh
[regex]::Replace($source, '\s*:\s*public\s+binary_function\s*<[^>]+>', '')
```

Files patched:
- `Lib/Own/TComp/TComp/TComp.cpp:10`
- `Client/TClient/TClientGame.cpp:394` and `412`
- `Client/TClient/TClientType.h:3446`
- `Client/TClient/TNewQuestDlg.cpp:10` and `18`
- `Client/TClient/TQuestNewDlg.cpp:17` and `25`

#### 2. `<algorithm>` not transitively included anymore
Modern MSVC stdlib doesn't pull `<algorithm>` via `<vector>` etc. `std::sort` / `std::binary_search` failed with "not a member of std" in 9+ files. Fix: add to `Client/TClient/StdAfx.h`:

```cpp
#include <algorithm>
#include <functional>
```

### Bonus: protocol assumptions confirmed by reading client source

The client's `Lib/Own/Engine Lib/Engine Lib/TachyonSession.cpp` confirms several open questions from PROTOCOL.md §5 by direct source-code comparison with `Server/TNetLib/Session.cpp`:

| # | Question | Resolution |
|---|----------|------------|
| Q1 | RC4 covers entire packet (incl. wSize)? | **YES** — line 222: `EncryptBuffer(CALG_RC4, pBuf, wSize, pBuf, wSize, ...)`. wSize is restored on line 224 (`pPacket->m_pHeader->m_wSize = WORD(dwBufSize)`), identical pattern to server's `Decrypt`. Our `Rc4Layer.TransformPacketPreservingWSize` matches. |
| Q2 | `g_strSecretKey` exact bytes? | **IDENTICAL** to server — line 12 of TachyonSession.cpp is literally `CString g_strSecretKey = "A5$$8AFS13A1::-11#!..'’1716AC&”/D1;;1#";`. Our `KeyTable.RawSecretKey` (39 bytes including NUL) matches the C++ `(GetLength() + 1) * sizeof(TCHAR) = 39` byte interpretation. |
| Q6 | RC4 key 128-bit or 40-bit? | **Implicit confirmation** — client and server both call `CryptDeriveKey(CALG_RC4, hCryptHash, CRYPT_EXPORTABLE, ...)` with identical args. Whatever the Windows CSP returns (likely 128-bit on Win10/11), both sides use the same key. If it worked on the original deployment, our 128-bit assumption is correct. |
| Q4 | `m_bUseCrypt` activation timing? | Client `TachyonSession::Encrypt` (line 196) doesn't check `m_bUseCrypt` — it always encrypts after the toggle. Same as server. Toggle point still needs to be located in handler code, but our "post-CS_LOGIN_ACK" assumption is consistent with the symmetric server-side flip. |

### Easter egg: `LOL.txt` debug leak

`TachyonSession::Encrypt` (lines 205-215) writes `g_strSecretKey` to a file named `LOL.txt` **on every encrypted packet send**:

```cpp
CStdioFile file;
if (file.Open(_T("LOL.txt"), CStdioFile::typeText | CStdioFile::modeCreate | CStdioFile::modeWrite))
{
    CString strMsg;
    strMsg.Format(g_strSecretKey);
    file.WriteString(strMsg);
    file.Close();
}
```

This is a forgotten developer trace, **shipped to production**. Side effects:
- The client's working directory must be writable.
- Real client deployments since 2014+ have likely been depositing `LOL.txt` on every player's machine on every packet send.
- Useful for byte-level verification: when running our build, `LOL.txt` will contain the literal bytes the running binary uses for the key. Final-mile confirmation of our reconstructed `KeyTable.RawSecretKey`.

### Where the built `TClient.exe` ended up

`TClient.vcxproj` sets `<OutDir>` to `..\..\..\4STORY_PW CLIENT 01\` — that's a sibling directory at `C:\Users\tomas\Desktop\Sources 5.0 (Araz)\Sources 5.0 (Araz)\4STORY_PW CLIENT 01\`. The build is standalone (no game assets next to it) so it won't render anything; to actually run it, copy the binary into an existing 4Story installation (replacing the production `TClient.exe`).

### What we did NOT verify

- The built `TClient.exe` actually starts. Running requires DirectX 9 init, MFC runtime, and either game asset files alongside or a properly configured 4Story install. **Untested.**
- Real wire traffic vs our server. Would require either patching the encrypted `4storyEU.ini` (we don't have the key) or DNS/hosts redirect to a known login server hostname (we don't know what the client expects).
- The `LOL.txt` claim — assumed true from source reading; not observed at runtime.
