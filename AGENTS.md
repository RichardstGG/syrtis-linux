# Syrtis agent routing

> `docs/knowledge/` is the canonical project knowledge base. This adapter routes work to it; it does not copy project facts.

Read [`docs/knowledge/README.md`](docs/knowledge/README.md) for every task before the task-specific document below. If `.agent-local/AGENTS.md` exists, read it after the canonical documents as additive machine-local guidance only; it must not override canonical architecture, verification, authorization, or release rules.

## Read order

| Task | Required reading |
|---|---|
| Any task | [`docs/knowledge/README.md`](docs/knowledge/README.md), then the task-specific row below |
| Rust, C ABI, or Swift data flow | [`docs/knowledge/architecture.md`](docs/knowledge/architecture.md) |
| Branches, reviews, merge, or release authorization | [`docs/knowledge/workflow.md`](docs/knowledge/workflow.md) |
| Tests, fixtures, cache invalidation, or cross-language checks | [`docs/knowledge/verification.md`](docs/knowledge/verification.md) |
| Shared tokscale engine or consumer pin | [`vendor/AGENTS.md`](vendor/AGENTS.md), [`vendor/README.md`](vendor/README.md), [`docs/knowledge/vendor-tokscale.md`](docs/knowledge/vendor-tokscale.md) |
| Release, Sparkle, appcast, or Homebrew | [`docs/knowledge/release.md`](docs/knowledge/release.md) |
| GitHub prose or contributor credit | [`docs/knowledge/communication.md`](docs/knowledge/communication.md) |
| Current maintenance priorities | [`docs/knowledge/current-state.md`](docs/knowledge/current-state.md) |

## Invariants

| Invariant | Rule |
|---|---|
| Source of truth | Add canonical project facts to `docs/knowledge/`; keep adapters as routing and guardrails. |
| Cross-language seam | Preserve the Rust -> C ABI -> Swift contract; verify both sides when a boundary changes. |
| Pre-aggregation | Do not attempt to remove a contribution after a mixed aggregate has been computed; pass the filter to the producer. |
| Shared-engine boundary | Land shared Rust changes in `tokscale-core`; advance only a reviewed gitlink, and keep app-owned FFI, C ABI, Swift, and build wiring in Syrtis. |
| Public repository | Never add private paths, credentials, machine-specific tooling details, or unpublished security work to tracked docs. |

## Authorization boundary

A plan approval authorizes implementation, not integration. Do not push, merge, tag, publish, or release without a separate explicit user instruction. Do not modify a dirty main checkout. A local `.agent-local/` overlay may hold machine-specific instructions and is intentionally outside the public knowledge base.

## Handoff

Before reporting completion, name the canonical files changed, run the relevant repository checks, and state any private material intentionally retained outside the repository. Use repository-relative links in documentation.

## For the Codex worker
- 只修改 issue 的 Scope 列出的路徑（Scope 只限制寫入，讀取不限）。
- 不要 git commit / push（.git 在 sandbox 中是唯讀的）。
- 不要安裝套件（沒有網路）。缺依賴就回報 blocked。
- 需要暫存目錄時：TMPDIR="$PWD/.tmp" <command>
- 完成後執行 ./scripts/ai/verify.sh --sandbox。被環境擋下的步驟（socket、D-Bus、網路）會印出 ENV_LIMITED，如實回報，不要視為失敗。
- 無法在 sandbox 驗證的項目（實機、CI）一律放進 QUESTIONS。
- 最終回覆只用以下格式，不要貼完整 diff：
  STATUS: done | blocked | partial
  SUMMARY: <3 行內>
  FILES: <改了哪些檔>
  TESTS: <跑了什麼、結果>
  ENV_LIMITED: <被環境擋下的測試，否則 none>
  COMMIT_MESSAGE: <conventional commit 一行>
  QUESTIONS: <blocked 或需人工驗證時要問的問題，否則 none>
