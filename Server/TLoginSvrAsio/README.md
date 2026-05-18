# TLoginSvrAsio — modernized login server (work-in-progress)

Parallel reimplementation of `Server/TLoginSvr/` on top of the
modernized TNetLib stack (Boost.Asio session loop, portable packet
codec, strongly-typed `MessageId` enum, OpenSSL crypto). Runs as a
separate binary; the legacy `TLoginSvr.exe` is untouched and can be
A/B-compared during migration.

## Status

This is Phase 3 step E of the modernization plan
(`_rewrite/docs/MODERNIZATION_PLAN.md`). Scope of the current
commit:

* Entry point + Asio-based listener
* Per-connection session driven by `tnetlib::AsioSession::RunPackets`
* MessageId-driven dispatcher
* One handler implemented as a stub: `CS_LOGIN_REQ` → `CS_LOGIN_ACK`
  with success result + placeholder fields
* Integration test (`test_tloginsvr_asio_handshake`) that connects
  a client `AsioSession`, sends a packet-codec-framed `CS_LOGIN_REQ`,
  asserts the `CS_LOGIN_ACK` round-trip

## Not yet ported

* All other 13 CS_/CT_ handlers from the legacy server
* Database lookups (`AuthService` equivalent — TLogin SP / TACCOUNT_PW)
* Connection registry + duplicate-session kick logic
* IP banlist / rate limiting
* Map-server endpoint resolution
* Session terminator + TCURRENTUSER cleanup
* RC4-over-everything client→server pre-pass — currently both peers
  use `PeerType::Server` (XOR only). Real legacy client compatibility
  requires composing `tnetlib_crypto::RC4MD5TransformCopy` into
  `AsioSession`. Tracked as Phase E.2.5.

## Run

After the next CMake configure + build:

```
$ ./build/bin/tloginsvr_asio --port 4815
TLoginSvrAsio listening on 0.0.0.0:4815
```

The legacy `TLoginSvr.exe` is unaware of this — they're independent
binaries listening on independent ports. Configure your test client
to point at whichever you want to A/B against.
