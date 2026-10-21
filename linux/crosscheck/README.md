# Linux C++ ↔ Swift cross-check (ADR-011)

| File | Source | SHA-256 |
|---|---|---|
| `fixtures/usage-pace.json` | Nanako0129/TokenBar-Windows `crosscheck/fixtures` @ 3dee020 (MIT) | `9a83616e…7806c` (matches that repo's canonical fingerprint) |
| `fixtures/format.json` | same | `6f67a77a…d6eb3` (matches) |
| `fixtures/provider-quota-pace-v3.json` | copy of this repo's Mac-owned `Fixtures/CrossCheck/provider-quota-pace-v3.json` | `a0d8090a…08654` |
| `diff.py` | Nanako0129/TokenBar-Windows `crosscheck/diff.py` @ 3dee020 (MIT) | |

Goldens in `golden/` are produced only by the macOS oracle workflow running the shipping Swift code; never hand-edit them.
