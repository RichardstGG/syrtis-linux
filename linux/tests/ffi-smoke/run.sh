#!/bin/sh
# Build the FFI static library, link the C smoke against it, and run it under a
# synthetic HOME so no real session, credential or IDE process is read.
set -eu

root=$(cd "$(dirname "$0")/../../.." && pwd)
target=${CARGO_TARGET_DIR:-$root/target}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cargo build --release --locked -p tb_core_ffi --manifest-path "$root/Cargo.toml"
cc -std=c11 -Wall -Werror -O1 -I"$root/Sources/CTB/include" \
    "$root/linux/tests/ffi-smoke/smoke.c" "$target/release/libtb_core_ffi.a" \
    -lgcc_s -lutil -lrt -lpthread -lm -ldl -lc -o "$work/smoke"

mkdir -p "$work/home/.claude/projects/smoke" "$work/tokscale"
now=$(date -u +%Y-%m-%dT%H:%M:%S.000Z)
printf '{"type":"assistant","timestamp":"%s","requestId":"req_1","sessionId":"s1","message":{"id":"msg_1","model":"claude-sonnet-4-5","role":"assistant","usage":{"input_tokens":100,"output_tokens":50,"cache_read_input_tokens":0,"cache_creation_input_tokens":0}}}\n' \
    "$now" >"$work/home/.claude/projects/smoke/s1.jsonl"

env -i PATH=/usr/bin:/bin HOME="$work/home" \
    XDG_CONFIG_HOME="$work/home/.config" XDG_DATA_HOME="$work/home/.local/share" \
    XDG_CACHE_HOME="$work/home/.cache" TOKSCALE_CONFIG_DIR="$work/tokscale" \
    TZ=Asia/Taipei "$work/smoke"
