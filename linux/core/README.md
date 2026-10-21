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

`AgentUsage` contains only the pace input types and `UsageWindow` decoder needed
by this selector, not the full AgentUsage surface. The decoder follows current
Swift v3 validation: `durationSeconds` comes from `paceStatus`, never legacy
`windowMinutes`, and the backend historical projection remains one coherent
result. Legacy fixtures therefore produce null or `rejected` results according
to the shipping decoder, regardless of their old explanatory notes.

From the repository root:

```sh
cmake -S linux/core -B linux/core/build -G Ninja
cmake --build linux/core/build
TZ=Asia/Taipei ctest --test-dir linux/core/build --output-on-failure
TZ=Asia/Taipei linux/core/build/crosscheck/syrtis-crosscheck linux/crosscheck/fixtures linux/core/build/actual format
```

The harness accepts `core-leaf`, `usage-pace`, `format`, and `provider-quota-pace-v3`. Like the
[Swift harness](../../Sources/CrossCheckHarness/main.swift), an omitted selector
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
exists in that root Swift harness. Unported selectors emit
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
