# Legacy-spec characterization tests

The legacy `Server/TMapSvr/` codebase **is** the gameplay specification —
there's no formal protocol document, the wire shape is implicit in the
handlers, and the behavioral edge cases (duplicate connect, suspender
flow, version mismatch, …) only exist in code. To port the ~621
handlers safely we write **one characterization test per legacy
handler**, structured so the test file alone documents every branch
the legacy code takes.

## Pattern

Every handler `OnCS_XXX_REQ` in `Server/TMapSvr/CSHandler.cpp` gets a
matching `tests/test_cs_xxx_req_spec.cpp` file that:

1. **Cites the legacy source.** A top-of-file comment block lists
   every branch the legacy handler can take, with file:line refs:

   ```
   // Source of truth: Server/TMapSvr/CSHandler.cpp:249-399
   //
   // Branches (legacy line → spec section):
   //   258-262  bad wVersion       → §1  reply CN_INVALIDVER + close
   //   298-299  checksum mismatch  → §2  close, NO ack
   //   301-302  m_dwID already set → §3  close, NO ack
   //   …
   ```

   The line numbers anchor the test to a specific code revision. When
   the legacy moves, the comment updates with it.

2. **One test function per branch.** Each function is named after
   the branch (`TestBadVersion`, `TestChecksumMismatch`, …) and
   contains a single arrange / act / assert block. Branches that
   need a mock peer (e.g. World server for the post-handshake
   `MW_CONRESULT_REQ` round-trip) build their fixture inline.

3. **Active vs PENDING.** Branches the modern handler already
   implements are **active** and must pass. Branches the modern
   handler doesn't yet implement are **PENDING** — wrapped in
   `void TestX_PENDING()` and called only behind a printf banner:

   ```
   std::printf("  PENDING  TestSuspenderFlow (CSHandler.cpp:333-355 not ported yet)\n");
   ```

   Pending tests don't count as failures. A `grep PENDING` across
   the tree gives an honest picture of what's left to port.

4. **No legacy linkage.** The test calls the modern
   `tmapsvr::MapServer` or `tmapsvr::Dispatch` directly — never the
   legacy `CTMapSvrModule`. That keeps Win32/ATL/MFC out of the test
   binary and means the suite cross-compiles to Linux.

5. **Wire-exact assertions where possible.** When the legacy
   response is a fixed shape (e.g. `CS_CONNECT_ACK { CN_INVALIDVER, 0
   }`), the test asserts byte-equality. When the response varies
   (e.g. server-id list in `CS_CONNECT_ACK` after a successful
   `MW_CONRESULT_REQ`), the test asserts the framing fields and
   leaves the variable payload to the mock-peer fixture.

## Workflow per handler

```
1. Read legacy handler end-to-end.
2. List branches as a comment block at top of test file.
3. For each branch:
   a. Write test (or PENDING stub).
   b. Run.  If active and modern passes  → keep.
            If active and modern fails  → record the diff in a
                                           `// MODERN-MISMATCH:` comment
                                           and either fix modern or
                                           downgrade test to PENDING.
4. Commit with reference to the legacy line range covered.
```

## Coverage tracking

The test files themselves are the coverage map. Branches counted via
`grep -c "  PASS\|  PENDING" tests/test_cs_*_spec.cpp` per file.

When a handler is fully active (no PENDING), it's considered ported.
The README's phase table is the running total.

## Mock peer fixtures

Handlers that need a peer (`MW_*`, `DM_*`, `SS_*`) get a tiny
in-process echo / scripted reply harness:

* `WorldPeerStub` — minimal Boost.Asio acceptor that replies with a
  pre-canned `MW_CONRESULT_REQ` to any `MW_ADDCHAR_REQ`.
* `DbPeerStub` — same shape for `DM_*` flows.

These live in `tests/peer_stubs/` and are reused across handler
tests.

## Naming

```
tests/
├── LEGACY_SPEC.md                    ← this file
├── test_cs_connect_req_spec.cpp      ← CSHandler.cpp:249
├── test_cs_conready_req_spec.cpp     ← CSHandler.cpp:402
├── test_cs_kickout_req_spec.cpp      ← CSHandler.cpp:417
├── test_cs_move_req_spec.cpp         ← CSHandler.cpp:439
├── …
└── peer_stubs/
    ├── world_peer_stub.{h,cpp}
    └── db_peer_stub.{h,cpp}
```

One file per legacy handler. The handler ID + file:line pair gives a
permanent, unambiguous anchor between legacy spec and modern test.
