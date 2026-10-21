# Linux-owned Swift oracle extension

This macOS SwiftPM package adds selectors that the
[root harness](../../../Sources/CrossCheckHarness/main.swift) does not provide.
The `core-leaf` selector covers ClientRegistry, ModelGrouping, ModelScope,
WindowResolver, and Grid with the shared synthetic fixture. Omitted selector or
`all` runs every registered extension in sorted order; unknown selectors fail.
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

`core-leaf` calls the shipping registry's explicit-input overloads and its
UserDefaults overloads. Its synthetic preferences are scoped to the harness
process and all four previous key values are restored after each case. It uses
the production `WindowMessage` decoder and `PerDay` initializer, and serializes
every grid cell, including inactive cells outside the requested year. The
self-test checks JSON output structure; semantic comparisons use `diff.py`
against the oracle output. Only the macOS oracle workflow produces goldens.

The `reports` extension reads the synthetic
[`reports.json`](../fixtures/reports.json) and calls the shipping
`TBCore.decodeEnvelope`, `decodeOptionalEnvelope`, and `JSONDecoder` for the
ported payload types. Package access permits testing the production envelope
decoder without invoking FFI or changing shipping Swift sources. Reflection is
used only to serialize decoded DTOs for comparison. Errors retain their kind,
coding path, missing field, and envelope contract description; bridge errors
retain their exact text. The public `FilterParityProbe.smokeSummary` exercises
the shipping status labels and POSIX cost formatting.

`check.py` checks `reports` using only `reports_check.verify_structure`: valid
JSON, one result per fixture case, matching ids, and the value/error output
shape. It never calls the C++ semantic assertions in `verify_cpp_output`, so a
new Swift classification cannot prevent oracle golden generation. Semantic
agreement is decided by `diff.py` against the generated golden.

On macOS, run the command above, then generate `reports.actual.json` explicitly
or with `all`. Build, structural checks, and comparison with C++ remain macOS CI
gates; Linux workers never create `golden/reports.json`.

The `agent-usage` extension reads
[`agent-usage.json`](../fixtures/agent-usage.json), decodes the shipping
`AgentUsagePayload`, `UsageWindow`, related values and `QuotaCurve`, and invokes
the production card/setup/configured-client and display-only rewrite methods.
`qualify` emits raw and qualified card IDs/labels from `rawCardWindows` and
`uniqueCardWindows`, so repeated-label qualification is explicitly covered here.
For reset-tier cases, `resetOffsetsSeconds` creates resets relative to `Date()`;
the fixture offsets leave at least fifty seconds before countdown rounding
changes. Only names and IDs are emitted for these cases, so no wall-clock value
enters the golden. Other cases serialize the full decoded shipping values.

`check.py` calls only `agent_usage_check.verify_structure` for this selector:
valid JSON, one result for each fixture case, matching IDs and output shape.
It never runs `verify_cpp_output` or guesses Swift semantic expectations.
The oracle workflow's existing `all` invocation includes this extension;
macOS build, structural preflight, golden generation and C++ comparison remain
CI gates. Workers never write `golden/agent-usage.json`.
