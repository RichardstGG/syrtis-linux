#!/bin/sh
# An empty environment keeps scanner overrides and real user data out of this test.
set -eu
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/home/.claude/projects/smoke" "$work/tokscale"
cat >"$work/home/.claude/projects/smoke/s1.jsonl" <<'EOF'
{"type":"assistant","timestamp":"2026-01-01T00:00:00.000Z","requestId":"req_1","sessionId":"s1","message":{"id":"msg_1","model":"claude-sonnet-4-5","role":"assistant","usage":{"input_tokens":100,"output_tokens":50,"cache_read_input_tokens":0,"cache_creation_input_tokens":0}}}
EOF
env -i PATH=/usr/bin:/bin HOME="$work/home" \
    XDG_CONFIG_HOME="$work/home/.config" XDG_DATA_HOME="$work/home/.local/share" \
    XDG_CACHE_HOME="$work/home/.cache" TOKSCALE_CONFIG_DIR="$work/tokscale" \
    QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software TZ=Asia/Taipei "$@"
