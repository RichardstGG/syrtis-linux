# Goldens (generated, do not hand-edit)

Produced by `.github/workflows/crosscheck-oracle.yml` run 39598792066 on commit b86132652577 (artifact `crosscheck-golden`), i.e. the shipping Swift code at that commit.

To refresh: run the workflow (workflow_dispatch or a PR touching linux/core or linux/crosscheck), download the artifact, and replace the files here in one commit that names the run id.

Note: 40 of the 42 `usage-pace` cases are `null` in the current Swift oracle; the strict v3 decoder rejects the legacy window shape (see docs/knowledge/verification.md, provider-wide v3 checkpoint).
