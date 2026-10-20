#!/usr/bin/env bash
# Mirrors the Linux-runnable subset of CI (docs/knowledge/verification.md).
# Swift/macOS gates cannot run here; workspace clippy and `cargo fmt` (main is not fmt-clean) stay human gates.
set -euo pipefail
cd "$(dirname "$0")/../.."

if [ "$(id -u)" = 0 ]; then echo "run as non-root (chmod-based tests fail as root)" >&2; exit 1; fi

mapfile -t sh_files < <(git ls-files --cached --others --exclude-standard '*.sh')
for f in "${sh_files[@]}"; do bash -n "$f"; done

cargo test -p tokscale-core --locked --offline
cargo test -p tb_core_ffi --locked --offline
# no -D warnings: mirrors linux.yml, and main has existing clippy warnings
cargo clippy -p tb_core_ffi --all-targets --locked --offline
echo "verify OK"
