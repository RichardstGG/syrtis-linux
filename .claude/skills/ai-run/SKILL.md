---
name: ai-run
description: Run one ai:ready GitHub issue through the Codex plugin, open a draft PR, and apply the user's review comments.
---

> 本 repo 規則：所有 `gh` 指令加 `--repo RichardstGG/syrtis-linux`；`git push`、`gh pr create`、開 issue/label 每次都要我明確同意（AGENTS.md Authorization boundary）。PR 由我自己 merge。

## Dispatch（一次一個 issue）
1. 確認 working tree 乾淨；不乾淨就停下來告訴我。
2. git fetch origin && git switch -c ai/issue-N origin/main
3. gh issue edit N --remove-label ai:ready --add-label ai:in-progress
4. 交給 codex:codex-rescue subagent，請求內容：
   「--background 依 AGENTS.md 的 Codex worker 規則完成 issue #N：<issue 全文>」
5. 記下 job id，用 `node "$(ls -d ~/.claude/plugins/cache/openai-codex/codex/*/ | sort -V | tail -1)scripts/codex-companion.mjs" status <job-id> --wait --timeout-ms 600000` 等待（逾時就再呼叫一次，最多 3 次）。
   完成後直接進入 Finalize；失敗或 3 次仍未完成 → 告訴我 job id 與狀態，停下來等我。

## Finalize（我說「看結果」或任務完成後）
1. 取得 /codex:result 的回報；STATUS 不是 done → 在 issue 留言貼出 QUESTIONS，標 ai:blocked。
2. ./scripts/ai/verify.sh；失敗 → /codex:rescue --resume 附上錯誤 log（最多 1 次）。
3. git add -A && git commit（用 COMMIT_MESSAGE）&& git push -u origin ai/issue-N
4. gh pr create --draft（body 加 🤖 標記與 SUMMARY/TESTS），label ai:review

## Apply review（我說「PR #X 有 comment」）
1. 讀取 review comments，整理成 fix brief（逐條列出檔案、行號、要改什麼；我標註「不用改」的略過）。
2. 同一個 Claude session 內：交給 codex:codex-rescue，請求「--resume <fix brief>」
   換過 session 時：改用「--fresh」，附上 issue、目前 diff、fix brief。
3. verify → commit（不 force push）→ push → 在 PR 留言摘要。

## Cleanup（PR merge 後）
git switch main && git pull && git branch -d ai/issue-N
