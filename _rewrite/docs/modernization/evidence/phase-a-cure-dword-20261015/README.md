# Native CURE DWORD arithmetic and defaults — 2026-10-15

Baseline: local `main`, `cde2ded`. **IMPLEMENTED / INTEGRATED_UNVERIFIED**.
Three connected source branches are added to the existing positive own-PC HP/MP
CURE path: DWORD product/subtraction modulo, explicit GetValue/Calculate defaults
for real BYTE modes, and resource-addition modulo with a living final target. Basic-party authority remains unchanged.
Full modernization and original-client compatibility remain unverified. Local
Git only; no push.

## Source DWORD calculation

Original `TSkillTemp.cpp:62–105` combines DWORD maxima with signed INT values.
The Win32/v141/MachineX86 source project fixes the original ABI. Windows
documents DWORD as unsigned32, and Microsoft documents preservation of bits
when converting unsigned integral values to signed types. The native helper
uses fixed-width integers and explicit `std::bit_cast<int32_t>` for that signed
projection. See [Windows types](https://learn.microsoft.com/en-us/windows/win32/winprog/windows-data-types)
and [Microsoft integral conversions](https://learn.microsoft.com/en-us/cpp/c-language/conversions-from-unsigned-integral-types?view=msvc-170).

MULTIPLY and PRECENT now retain the low32 bits of the actual unsigned product.
Two DWORD factors fit in a uint64 intermediate; narrowing explicitly performs
modulo2^32. The unsigned subtraction likewise preserves low32 bits before
interpreting the result as Windows INT. DIVIDE keeps its nonpositive-divisor
replacement with1, then applies the same quotient/subtraction projection.
PRECENT divides the already reduced DWORD product by double100 and truncates
that total before subtracting the base. A widened percentage product would
change the source expression.

Only the projected signed final delta in0..INT32_MAX/15 is admitted. The source
signed variance product must remain representable for every roll. A negative
projection or unsafe positive magnitude remains unavailable. DECREASE still
refuses INT_MIN negation, and Calc2 requires finite representable conversion.

Actual HP/MP DWORD additions now also preserve low32 bits after each sampled
row. All rows run before one target-level clamp. Intermediate HP0 may be raised
by a later HP row; there is no source mid-row death check. After all rows and
clamp, final HP0 is refused before zero-MP cleanup, packets or durable writes.
This explicitly leaves the source LASTHIT/HPMP/Defend-death producer OPEN. Each
target must finish alive; a later target cannot repair an earlier target's death.
MP0 remains valid and uses existing source cleanup. Negative final deltas remain
outside this positive capability.

Existing core writes preserve DWORD HP/MP bits through signed INTEGER bindings,
and existing finish before/after bars use bounded positive BIGINT values.
Checkpoint fingerprints, graph state and load projection retain those bits;
no persistence format changes. Unsupported final death rolls back the Finish
transaction without refunding/replaying an earlier accepted ACT.

For source algebra illustration, MULTIPLY base0x80000001/value3 gives final
delta2 after the product wraps. Base0xfffffff0/value0 gives delta16 after
subtraction. These examples were not executed and do not establish that an
actual admitted actor can reach those maxima.

## Actual default branches and row lifecycle

GetValue's original default returns0 for a Calc BYTE outside0..3. The selected
combine still runs; its final delta can therefore be nonzero after DWORD
projection. Calculate's separate default leaves final delta0, but it always
evaluates GetValue first. Invalid known Calc2 cannot bypass its checks by using
an unrecognized combine.

The ACT reader now preserves these actual bytes without its previous mode
whitelist. It still requires a real nonempty positive SA_ONCE SDT_CURE HP/MP
profile, actual item/template/current-stat/graph authority and pinned catalogs.
Missing data is never replaced with a default row. Formula34 and outgoing growth
metadata remain required for every actual Calc2 row, including one with a
default combine.

Init and ACT preflight use the same helper as every normal native FINISH row.
Current rank/maxima are rederived at each stage. Real zero/default rows retain
their RNG draw, PerformSkill/DEFEND, zero-MP cleanup and one-use completion;
HPMP is emitted only for actual resource changes. Actual MISS and empty FINISH
skip numeric evaluation. Existing actual802 lifecycle, equipment cleanup,
current owner/session/epoch/route/state proof, checkpoint/fullgraph atomicity,
party/recipient projection fences and no-replay policy remain unchanged.
There is no new schema, publisher, grant, cost, receipt or wire field.

## Evidence and deferred acceptance

- [Developer contract](map-dword.md) and [independent source audit](analysis.md).
- [Shared calculation helper](../../../../../Server/TMapSvrAsio/services/native_cure.h)
  and [actual ACT reader](../../../../../Server/TMapSvrAsio/services/postgresql_map_actitems.cpp).
- [Unchanged FINISH consumer](../../../../../Server/TMapSvrAsio/services/postgresql_map_checkpoint.cpp)
  and [coverage inventory](../../legacy-modern-coverage.csv); whole-server
  `ALL.RESPONSIBILITIES` remains `NOT_ANALYZED`.

Verification is limited to static original source, types/API/callers, SQL text,
packet/client consumers, official compiler documentation, CSV, Markdown links
and Git diffs. The CSV has197 data rows,12 columns, unique IDs and LF endings;
the whole-server row is unchanged. All15 added local Markdown targets exist,
and `git diff --check` is clean. No build, test, SQL, migration, import, backup extraction, service,
server, client or container ran. Original sources/client/protocol/backups,
applied001–043, existing written044–072, older evidence and private audits are
unchanged. Written044–072 remain unapplied; next073. No actual live data pair
or original executable was certified.

Phase B must cover modulo products, subtraction and signed-bit boundaries;
percent product-before-division; DIVIDE clamp/quotient; final variance and per-row
resource wrap, intermediate HP0 recovery, final HP0 refusal and MP0 cleanup;
every source default with known/unknown counterparts; invalid
Calc2 plus default combine; ordered zero/default row RNG and cleanup; actual
current-rank/max changes, item/deadline/receipt behavior, MISS/empty/actual802,
ordinary/loop completion, live party output and crash/recovery/handoff without
replaying an uncertain commit or output. Original DEFEND84 and HPMP21 consumers
at `Client/TClient/CSHandler.cpp:2806,3492` retain their existing fields.

Negative final deltas, final-HP0 Defend death, unsafe signed operations,
outgoing128/229/3604, foreign/linked/repeated/richer effects, guild/companion/
recall graphs, general combat, source DB-only effect-save parity, CRT/libm
boundaries and full Map admission remain OPEN.
