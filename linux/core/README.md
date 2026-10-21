# Syrtis Core

Standalone C++20 static library, linked only to Qt6::Core. This skeleton has no
ported calculation surfaces yet. Future ports retain their Swift source names
and semantics; the library does not perform I/O, call FFI, or aggregate Rust data.
It is intentionally not included by [`../CMakeLists.txt`](../CMakeLists.txt).

From the repository root:

```sh
cmake -S linux/core -B linux/core/build -G Ninja
cmake --build linux/core/build
ctest --test-dir linux/core/build --output-on-failure
TZ=Asia/Taipei linux/core/build/crosscheck/syrtis-crosscheck linux/crosscheck/fixtures linux/core/build/actual format
```

The harness accepts `usage-pace`, `format`, and `provider-quota-pace-v3`. Like the
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
