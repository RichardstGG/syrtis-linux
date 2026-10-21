# Linux-owned Swift oracle extension

This macOS SwiftPM package adds selectors that the
[root harness](../../../Sources/CrossCheckHarness/main.swift) does not provide.
The `quota-trend` selector calls shipping `QuotaHeatmapFold` and `QuotaTrendFold`
with the synthetic [fixture](../fixtures/quota-trend.json), decoding curve points
with the production `QuotaCurvePoint` decoder. Omitted selector or `all` runs
every registered extension in sorted order; unknown selectors fail.
The root harness remains responsible for `usage-pace`, `format`, and
`provider-quota-pace-v3` (its legacy `all` runs only the first two).

`TokenBarCore` and `CTB` are directory symlinks to the shipping sources.
`Sources/LinuxCrossCheckHarness/Format.swift` likewise links to the app's
formatter. No shipping source is copied or modified. The full core includes
the FFI facade, so this package links the same Rust static archive and system
libraries as the root harness. It has no external SwiftPM dependencies.

From the repository root on macOS, after `cargo build --release --locked`:

```sh
swift build --package-path linux/crosscheck/swift
bin_dir=$(swift build --package-path linux/crosscheck/swift --show-bin-path)
python3 linux/crosscheck/swift/check.py "$bin_dir/linux-crosscheck-harness" "$TMPDIR"
TZ=Asia/Taipei swift run --package-path linux/crosscheck/swift \
  linux-crosscheck-harness linux/crosscheck/fixtures /tmp/crosscheck-actual \
  all -AppleLanguages "(en)"
```

Add extension handlers to `selectors` in `main.swift`; each receives the fixture
directory and returns a JSON object using shipping calculations and production
decoders. Do not re-aggregate Rust data. The harness writes sorted-key
`<selector>.actual.json`, with the root harness's timezone and English pin.
Use only synthetic fixtures.

The [oracle workflow](../../../.github/workflows/crosscheck-oracle.yml) runs both
harnesses, renames outputs to `<selector>.json`, and uploads one
`crosscheck-golden` artifact. Goldens are never fabricated by the Linux harness.
