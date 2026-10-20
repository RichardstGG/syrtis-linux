#!/usr/bin/env bash
# Mirrors the Linux-runnable subset of CI (docs/knowledge/verification.md).
# Swift/macOS gates cannot run here; workspace clippy and `cargo fmt` (main is not fmt-clean) stay human gates.
set -euo pipefail
cd "$(dirname "$0")/../.."

if [ "$(id -u)" = 0 ]; then echo "run as non-root (chmod-based tests fail as root)" >&2; exit 1; fi

mapfile -t sh_files < <(git ls-files --cached --others --exclude-standard '*.sh')
for f in "${sh_files[@]}"; do bash -n "$f"; done

# shortcut: tb_core_ffi does not build on Linux until PR-01 (feat/linux-rust-core) merges; add its test+clippy lines then.
cargo test -p tokscale-core --locked --offline
echo "verify OK"
