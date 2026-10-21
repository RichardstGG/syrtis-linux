# Claude quota credentials on Linux (PR-04 spike, issue #29)

**Date**: 2026-10-21. **Host**: Debian 13, KDE Plasma 6 Wayland. **Method**: existence checks, file modes, process and D-Bus name listing, and reading `crates/tb_core_ffi/src/agent_usage.rs`. No credential file was opened and no token value was read, printed, or stored.

## What the Rust provider already does on Linux

`fetch_claude_inner` walks a fixed chain for the primary account; on Linux the Keychain steps are compiled out, leaving:

| Source | Code | Refresh write-back | Linux status |
|---|---|---|---|
| `CLAUDE_CODE_OAUTH_TOKEN` (process env, or a login shell's env) | `ClaudeCredentialSource` env | none (read-only, no refresh token) | works; awkward for a desktop-launched app |
| `~/.claude/.credentials.json` (Claude Code CLI login) | `ClaudeCredentialSource::File` | **yes**: a rotated refresh token is written back to this file | works; absent on this host |
| macOS Keychain `Claude Code-credentials` / `tokenbar-claude-oauth-token` | `cfg(target_os = "macos")` | Keychain | not applicable |

When none is present the card reports `unconfigured` with `CLAUDE_UNCONFIGURED_ERROR`, which is the PR-00 observation.

## Observations on this host

- Claude Code CLI is installed (`~/.local/bin/claude`), but there is no `~/.claude/.credentials.json`: the CLI has not been logged in here.
- The maintainer's login is the Claude desktop app (Electron, `~/.config/Claude/`). Its session state lives in the app's own storage, protected by the desktop secret store.
- A Secret Service provider is running (`org.freedesktop.secrets`, served by gnome-keyring and KWallet 6). `secret-tool` (package `libsecret-tools`) is **not** installed.

## Options

| | Source | Code needed | Fit with PRD NFR-02 ("read only supported existing sources; do not rewrite other CLIs' login data") |
|---|---|---|---|
| A | `claude` CLI login → `~/.claude/.credentials.json` | none | Reading is fine. **Refresh write-back rewrites Claude Code's file**, which NFR-02 forbids unless the maintainer accepts it (the same decision was made for Codex's `~/.codex/auth.json` on 2026-10-15). |
| B | `claude setup-token` stored by the user in the Secret Service under a Syrtis-owned item, read via `secret-tool lookup` (mirrors the macOS `tokenbar-claude-oauth-token` Keychain item, which shells out to `security`) | Linux cfg branch in `agent_usage.rs`; runtime dependency on `libsecret-tools` | Fully compliant: the item is Syrtis-owned, the token is long-lived, no write-back. |
| C | `CLAUDE_CODE_OAUTH_TOKEN` in the session environment (e.g. `~/.config/environment.d/`) | none | Compliant, but the token sits in plain text in a config file and every process inherits it. |
| D | Read the Claude desktop app's own session store | decrypting another app's storage | **Out of bounds**: not a supported source, breaks NFR-02, and is fragile across app updates. Not recommended. |

## Recommendation

Support **A and B**, with B as the documented default:

1. A needs no code. Ship it once the maintainer accepts the refresh write-back to `~/.claude/.credentials.json` (or disable write-back on Linux and let the CLI refresh its own file).
2. B is implemented in issue #30 as an additive `cfg(target_os = "linux")` lookup of a fixed Syrtis-owned Secret Service item, which mirrors the macOS setup-token path. It must fail closed when `secret-tool` is missing and must never put the token on argv or in logs.
3. The unconfigured message on Linux should name both routes instead of the Keychain.

## Maintainer decision

_Pending:_
- Is B (Secret Service setup-token) accepted?
- For A, is refresh write-back to `~/.claude/.credentials.json` accepted?
- Is `libsecret-tools` an acceptable runtime dependency for the `.deb` (PR-08)?
