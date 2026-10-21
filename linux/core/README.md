# Syrtis Core

Standalone C++20 static library, linked only to Qt6::Core. Ports retain their
Swift source names and semantics; the library does not perform I/O, call FFI,
or aggregate Rust data. The leaf ports are `ClientRegistry`, `ModelGrouping`,
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

From the repository root:

```sh
mkdir -p .tmp
TMPDIR="$PWD/.tmp" cmake -S linux/core -B linux/core/.tmp/build -G Ninja
TMPDIR="$PWD/.tmp" cmake --build linux/core/.tmp/build
TMPDIR="$PWD/.tmp" ctest --test-dir linux/core/.tmp/build --output-on-failure
TZ=Asia/Taipei linux/core/.tmp/build/crosscheck/syrtis-crosscheck linux/crosscheck/fixtures linux/core/.tmp/actual core-leaf
```

The harness accepts `core-leaf`, `usage-pace`, `format`, and `provider-quota-pace-v3`. Like the
[Swift harness](../../Sources/CrossCheckHarness/main.swift), an omitted selector
or `all` runs the two legacy selectors (`usage-pace` and `format`). `TZ` must be
exactly `Asia/Taipei`. The Qt harness pins its locale to English internally;
the Swift-compatible `-AppleLanguages "(en)"` pair is also accepted anywhere in
the arguments. Other options and non-English language pins fail closed.

Each selector writes `<selector>.actual.json`. Unported selectors emit
`{"error":"not ported"}`; their CTests skip. Once ported, each CTest compares
against the matching macOS oracle JSON under [`../crosscheck/golden/`](../crosscheck/golden/),
skipping if that oracle is absent. The runner copies the oracle to an isolated
build directory with the `.actual.json` suffix required by the vendored
[`diff.py`](../crosscheck/diff.py); it never writes the oracle directory.
Only the deliberately synthetic self-test pair lives under
[`../crosscheck/harness/selftest/`](../crosscheck/harness/selftest/).

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
