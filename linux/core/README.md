# Syrtis Core

Standalone C++20 static library, linked only to Qt6::Core. `UsagePace` ports the
shipping Swift linear/historical mode policy, stage thresholds, ETA, risk and
countdown text. `Localization` preserves English keys and semantic fallbacks,
with the string/integer format placeholders used by this surface. Future ports
retain their Swift source names and semantics; the library does not perform
I/O, call FFI, or aggregate Rust data.

The leaf ports are `ClientRegistry`, `ModelGrouping`,
`ModelScope`, `WindowResolution`, and `Grid`. `Grid.h` includes its `ISODay` and
`PerDay` value dependencies from `UsageStats.swift`. `WindowResolution.h` carries
the message timestamp needed by the resolver.

`ClientPreferences` is a caller-supplied snapshot of the three registry defaults
and optional legacy order. Missing order and an explicitly saved empty order
remain distinct. The settings owner loads the snapshot and persists it after
`migrateLegacyOrderKey`; Core only changes the snapshot. The explicit raw-string
and set overloads match Swift's reactive overloads. Tab hiding folds and expands
groups; limits hiding remains member-specific.

`QuotaHeatmap` and `QuotaTrend` retain their Swift source names and semantics.
It is intentionally not included by [`../CMakeLists.txt`](../CMakeLists.txt).

`Format` ports the English-pinned formatting surface from
[`Format.swift`](../../Sources/Syrtis/Format.swift): compact/exact tokens, USD,
month/day labels, relative time and local day keys. Its `todayTokens`,
`todayCost` and `todayTopClient` functions implement the format selector's
unfiltered [`Graph.swift`](../../Sources/TokenBarCore/Graph.swift) projection:
read the last matching contribution's existing totals, and select its top
client using Swift's saturating stripe fold and token/cost/id tie-break.
`paceDurationText` delegates to the existing `UsagePace` formatter. Numeric
formatting rounds the binary Double directly, independently of the host locale.

`AgentUsage` ports the shipping payload, snapshot, identity, credits, transport
diagnostic, pace and window decoders, plus setup/configured-client selection,
card qualification and display-only identity/history rewrites. `durationSeconds`
comes from `paceStatus`, never legacy `windowMinutes`, and the backend historical
projection remains one coherent result. `UsageWindow::decode(QJsonValue)` keeps
the optional-result interface used by UsagePace; the throwing Decoder overload
preserves error kinds and paths for typed payload/envelope decoding. Provider
calls and transport logging remain outside this UI-free library.

`QuotaCurve` validates producer-owned points and coverage without reconstructing
history: required active flags, durations within 400 days, cycle bounds with
checked subtraction, percentages, ordering, exact duplicate samples, coverage
endpoints/count and the 65,536-point ceiling. It preserves `isActiveGroup` rather
than deriving it from reset timestamps.

`AccountIdentity` keeps client and account keys separate, shares window IDs
between overview rows and heatmaps, and lets the settings owner install the
captured Antigravity label resolver. `QuotaResolver` preserves unmatched
explicit selections during partial fetches, migrates unique raw labels, and
resolves explicit accounts independently of Auto's client exclusions. Auto
considers every healthy account, preserving payload order on percentage ties.

`QuotaSummaryFold` reuses that resolver and `UsagePace` presentation for its
tightest reading, comfortable-window counts, largest burn deficit and number
of windows actually checked. `QuotaOverviewFold` consumes caller-supplied
cycles, showing the newest sixteen in chronological order while deriving
exhaustion from the lifetime peak (99% or above). `QuotaHistory.h` contains only
the `QuotaCycle` value dependency from Swift, without history folding or joins.

From the repository root:

```sh
cmake -S linux/core -B linux/core/build -G Ninja
cmake --build linux/core/build
TZ=Asia/Taipei ctest --test-dir linux/core/build --output-on-failure
TZ=Asia/Taipei linux/core/build/crosscheck/syrtis-crosscheck linux/crosscheck/fixtures linux/core/build/actual format
```

The harness accepts `core-leaf`, `usage-pace`, `format`, `provider-quota-pace-v3`,
`quota-trend`, `reports`, and `agent-usage`. Like the [Swift harness](../../Sources/CrossCheckHarness/main.swift), an omitted selector
or `all` runs the two legacy selectors (`usage-pace` and `format`). `TZ` must be
exactly `Asia/Taipei`. The Qt harness pins its locale to English internally;
the Swift-compatible `-AppleLanguages "(en)"` pair is also accepted anywhere in
the arguments. Other options and non-English language pins fail closed.

Each selector writes `<selector>.actual.json`. `usage-pace` implements `compute`
and `runOutRisk` with the shipping Swift harness's output fields, nulls and
rejections. Its existing Swift mode in `Sources/CrossCheckHarness/main.swift`
remains the oracle; no new selector or Swift mode is introduced. `format` handles
all eleven existing Swift harness functions, including null top-client results
and errors for missing or invalid `now` timestamps. Its oracle mode also already
exists in that root Swift harness. `provider-quota-pace-v3` uses the existing
root Swift oracle mode too: typed production payload/window decoding, raw
lifecycle rows, pace presentation, selection migration, legacy and malformed
windows. The current read-only fixture contains fourteen cases and nine
lifecycle windows. `quota-unit` additionally checks account identity, Auto
exclusions and ties, explicit account selection, summary counts/burn warnings,
and overview strip/peak boundaries; those extra checks are C++ regressions,
not independent Swift oracle comparisons. Unported selectors emit
`{"error":"not ported"}`; their CTests skip. Once ported, each CTest compares
against the matching macOS oracle JSON under [`../crosscheck/golden/`](../crosscheck/golden/),
skipping if that oracle is absent. The runner copies the oracle to an isolated
build directory with the `.actual.json` suffix required by the vendored
[`diff.py`](../crosscheck/diff.py); it never writes the oracle directory.
Only the deliberately synthetic self-test pair lives under
[`../crosscheck/harness/selftest/`](../crosscheck/harness/selftest/).
`usage-pace-unit` covers calculation/rounding boundaries and presentation;
`crosscheck-selftest` also runs synthetic v3 inputs through the real selector
and checks strict decoding and serialization without creating oracle goldens.
The same self-test checks Format's binary rounding, Int64 limits, local midnight,
duplicate days, per-client stripe totals, saturation and deterministic ties.

The synthetic [`core-leaf.json`](../crosscheck/fixtures/core-leaf.json) covers
registered and fallback styles, aliases, grouped tab/limit visibility, ordering,
reorder and migration guards, model folding and scope token matching, window
anchor boundaries, leap/century and cross-year grid cells, and exact Int64
values beyond binary64's integer range. The handler writes every grid cell.
`core-leaf-selftest` checks selected branch results and fixture failures without
creating a golden. `crosscheck-core-leaf` uses the shared runner and `diff.py`,
and returns CTest SKIP (77) until the macOS oracle supplies `core-leaf.json`.
Its CTest registration lives here to keep the crosscheck parent outside this
port's write scope. The matching Swift extension invokes shipping Swift code;
its build, self-test, and oracle comparison remain macOS CI gates.

`quota-trend` covers both folds using the synthetic
[fixture](../crosscheck/fixtures/quota-trend.json). Heatmap inputs are the three
fields consumed from each curve point (seconds); its weekday/hour cells and
observed days use a Gregorian calendar in the supplied `QTimeZone`. Trend samples
use milliseconds. The default-constructed `QuotaHeatmap{}` is Swift's `.empty`.
`quota-trend-selftest` checks local semantics independently of oracle availability;
`crosscheck-quota-trend` skips until the macOS oracle supplies its golden. The
[Swift extension](../crosscheck/swift/README.md) calls the shipping folds for
parity validation; local assertions do not substitute for that gate.

`reports` ports the payload declarations and pure envelope decoders from
[`TBCore.swift`](../../Sources/TokenBarCore/TBCore.swift), plus `AgentsReport`,
`HourlyReport`, `ModelReport`, and `FilterParityProbe`. `TBCore::decodeEnvelope<T>`
and `decodeOptionalEnvelope<T>` accept the bridge's `QJsonObject` (through
`QJsonValue`) or copied JSON bytes. No FFI symbols or pointer ownership enter
Core. This port covers decoding and the parity probe's bounded smoke summary;
model display folding and other report calculations are outside this decoder
surface. Reports retain Rust's supplied totals, entry order, and client lists.

`DecodingError` records Swift's error kind, coding path, and missing field;
`TBCoreError` preserves backend error text, including the `unknown` fallback.
The typed payload and optional error are decoded before checking `ok`, as in
Swift. Missing and explicit-null `data` differ only in the optional-envelope
path. Unknown keys are ignored; required fields never default to zero.
Missing required keys produce `keyNotFound`. Present null values produce
`valueNotFound`, except keyed `Bool` fields, where Swift's direct type check
produces `typeMismatch`. Optional fields accept both missing and null keys.

[`reports.json`](../crosscheck/fixtures/reports.json) contains only synthetic
data, including legacy optional fields, exact signed 64-bit values, malformed
envelopes, all four parity statuses, and the Rust `msPer1kTokens` spelling.
`reports-selftest` checks C++ decoder regressions, including missing/null/wrong
types for every Boolean field and each other decoded field category. The Swift
oracle preflight shares only JSON structure and case-id coverage checks;
semantic agreement is checked by `diff.py` after golden generation.
`crosscheck-reports` runs the normal comparator and skips with return code 77
until macOS supplies `golden/reports.json`.

The [`agent-usage.json`](../crosscheck/fixtures/agent-usage.json) selector covers
payload/window/curve decoding, nested malformed values, legacy and v3 states,
setup placeholders, account-history rewrites, and repeated-label qualification.
It serializes both raw and qualified card names: unlike the legacy Swift/C#
harness, this selector explicitly exercises the presentation layer discussed in
[verification.md](../../docs/knowledge/verification.md#cross-port-fixture-cross-check).
Duplicate card IDs keep the first row; repeated labels try distinct duration
names, distinct reset spans, then collision-free ordinals, preserving raw labels
and identities. Swift String canonical equivalence is retained for grouping.

Qualification fixtures with `resetOffsetsSeconds` materialize synthetic resets
relative to the harness clock, and emit only card IDs and names. Offsets sit ten
seconds below minute boundaries (at least fifty seconds of margin before the
countdown changes), so the macOS oracle can exercise shipping
`uniqueCardWindows` without exposing a clock override in production Swift.
C++ uses a fixed reference clock with the same offsets. Other operations retain
all decoded fields and backend totals. The Swift preflight checks only output
JSON structure and case IDs; C++ semantic regression assertions are separate.
`agent-usage-selftest` also constructs a curve above the sample ceiling locally.
`crosscheck-agent-usage` uses `diff.py` and skips (77) until the macOS workflow
supplies its golden. Workers never create oracle goldens.
