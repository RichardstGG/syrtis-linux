"""Focused leaf-model checks; these assertions are not macOS oracle goldens."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def verify_output(path):
    out = json.loads(Path(path).read_text(encoding="utf-8"))
    assert out["allIds"] == sorted(set(out["allIds"]))
    assert len(out["allIds"]) == 43
    styles = {s["id"]: s for s in out["styles"]}
    assert set(out["allIds"]) <= styles.keys()
    assert styles["claude"]["color"] == "#d97706"
    assert styles["claude"]["shortName"] == "Claude"
    assert styles["antigravity-cli"]["shortName"] == "Antigravity CLI"
    assert styles["antigravity-cli"]["quotaOwner"] == "antigravity"
    assert styles["grok-bot"]["tabSlice"] == ["grok-bot"]
    assert styles["grok"]["tabDisplayName"] == "Grok Build & Bot"
    assert styles["gemini-cli"]["canonicalClient"] == "gemini"
    assert styles["unknown-cli"]["displayName"] == "Unknown-cli"
    assert styles["unknown-cli"]["canonicalClient"] == "unknown-cli"
    assert styles["unknown-cli"]["color"] == "#6b7280"
    assert styles[""]["displayName"] == ""
    assert styles["ßeta"]["displayName"] == "SSeta"
    assert styles["e\u0301clair"]["displayName"] == "E\u0301clair"
    assert styles["🧑\u200d💻dev"]["displayName"] == "🧑\u200d💻dev"
    assert styles["Unknown Code"]["shortName"] == "Unknown"
    assert styles["Unknown CLI"]["shortName"] == "Unknown"
    assert out["labels"] == ["codex", "claude", "copilot", "antigravity", "openai", "gemini", "", "mixed"]
    cases = out["registry"]
    assert cases[0]["parseIdList"] == cases[0]["displayClients"] == []
    c = cases[1]
    assert c["parseIdList"] == ["claude", "codex", "claude", " codex"]
    assert c["parseIdSet"] == [" codex", "claude", "codex"]
    assert c["tabClients"] == ["grok", "codex", "antigravity", "unknown", "claude"]
    assert c["withGroupMembers"] == ["grok-bot"]
    assert c["hiddenTabClients"] == ["grok", "grok-bot"]
    assert c["quotaExcludedClients"] == ["antigravity-cli", "grok", "grok-bot"]
    assert c["knownLimitsClients"] == ["grok-bot", "codex", "antigravity-cli", "antigravity"]
    assert c["orderedClients"] == ["antigravity-cli", "antigravity", "codex", "unknown", "grok", "grok-bot", "claude"]
    assert c["displayClients"] == ["antigravity", "codex", "unknown", "antigravity-cli", "claude"]
    assert c["quotaClients"] == ["antigravity-cli", "antigravity", "codex", "unknown", "claude"]
    assert c["settings"]["hiddenClients"] == ["grok-bot"]
    assert c["settings"]["hiddenLimitsClients"] == ["antigravity-cli"]
    assert c["settings"]["hiddenTabClients"] == ["grok", "grok-bot"]
    assert c["settings"]["quotaExcludedClients"] == c["quotaExcludedClients"]
    assert c["settings"]["orderedClients"] == ["grok", "grok-bot", "codex", "antigravity-cli", "unknown", "claude", "antigravity"]
    assert c["settings"]["displayClients"] == ["codex", "antigravity-cli", "unknown", "claude", "antigravity"]
    assert cases[2]["hiddenTabClients"] == ["antigravity", "antigravity-cli"]
    assert cases[2]["quotaExcludedClients"] == ["antigravity", "antigravity-cli", "grok-bot"]
    assert cases[3]["quotaClients"] == ["antigravity-cli", "grok-bot", "antigravity"]
    assert cases[4]["orderedClients"] == ["codex", "claude", "claude", "new"]
    assert cases[5]["displayClients"] == cases[5]["quotaClients"] == []
    assert [c["migratedOrder"] for c in cases[:5]] == [None, "codex,claude", "", "codex", None]
    assert all(c["migratedOrder"] == c["migratedOrderAgain"] for c in cases)
    assert cases[6]["reorder"] == ["b", "c", "a"]
    assert cases[6]["mergeReorder"] == ["off", "b", "middle", "c", "tail", "a"]
    assert cases[7]["reorder"] == ["c", "a", "b"]
    assert cases[7]["mergeReorder"] == ["off", "c", "middle", "a", "tail", "b"]
    for c in cases[8:11]:
        assert c["reorder"] == ["a", "b", "c"]
        assert c["mergeReorder"] == ["off", "a", "middle", "b", "tail", "c"]
    assert cases[11]["reorder"] == ["b", "a", "c"]
    assert cases[11]["mergeReorder"] == ["b", "a", "off"]
    assert out["grouping"][:3] == ["grok-4", "grok-4.20", "grok-04.002"]
    fixture = json.loads((Path(__file__).resolve().parents[1] / "fixtures/core-leaf.json").read_text())
    assert out["grouping"][3:] == fixture["grouping"][3:]
    assert out["scopes"] == [True, True, False, False, False, False, True, False, True, True, True]
    windows = out["windows"]
    assert [w["kind"] for w in windows[:13]] == [
        "unavailable", "unavailable", "active", "unavailable", "active", "idle",
        "inferred", "inferred", "unavailable", "idle", "unavailable", "unavailable", "inferred"]
    assert (windows[2]["start"], windows[2]["end"]) == (100, 110)
    assert (windows[4]["start"], windows[4]["end"]) == (95, 105)
    assert (windows[6]["start"], windows[6]["end"]) == (100, 110)
    assert (windows[7]["start"], windows[7]["end"]) == (95, 105)
    assert (windows[12]["start"], windows[12]["end"]) == (125, 135)
    assert [w["firstUsageAfterReset"] for w in windows[13:]] == [None, None, 100, 101, 9007199254740992, 9007199254740993]
    grids = out["grids"]
    assert grids[0] == {"cols": 53, "rows": 7, "cells": [], "maxTokens": 0, "perDayIsActive": []}
    assert [g["cols"] for g in grids] == [53, 53, 54, 53, 54, 53]
    for g in grids[1:]:
        assert len(g["cells"]) == g["cols"] * 7
        assert g["rows"] == 7
        assert [(c["col"], c["row"]) for c in g["cells"]] == [(col, row) for col in range(g["cols"]) for row in range(7)]
    assert grids[2]["cells"][0]["date"] == "1999-12-26"
    assert grids[3]["cells"][0]["date"] == "1899-12-31"
    grid = grids[-1]
    cells = {c["date"]: c for c in grid["cells"]}
    assert grid["maxTokens"] == 9007199254740993
    assert cells["2023-12-31"]["tokens"] == 9999
    assert not cells["2023-12-31"]["inYear"] and not cells["2023-12-31"]["active"]
    assert cells["2024-02-29"]["active"] and cells["2024-02-29"]["cost"] == 0.5
    assert not cells["2024-03-01"]["active"] and cells["2024-03-01"]["cost"] == 0.25
    assert cells["2024-03-02"]["tokens"] == -3 and not cells["2024-03-02"]["active"]
    assert cells["2025-01-01"]["tokens"] == 9223372036854775807
    assert not cells["2025-01-01"]["active"]
    assert grid["perDayIsActive"] == [True, True, True, True, False, True, True]
    assert out["days"][0] == {"number": 0, "iso": "1970-01-01", "weekday": 4}
    assert out["days"][1] == {"number": -1, "iso": "1969-12-31", "weekday": 3}
    assert out["days"][5]["iso"] == "2025-01-01"
    assert out["days"][6]["iso"] == "2024-03-01"
    assert out["days"][7] is None and out["days"][9] is None
    assert out["days"][8]["iso"] == "2024-01-02"
    assert out["dayNumbers"][0]["iso"] == "-001-12-31"
    assert [d["weekday"] for d in out["dayNumbers"][:6]] == [5, 6, 0, 3, 4, 5]


def main():
    harness, work = sys.argv[1:]
    here = Path(__file__).resolve().parent
    fixtures = here.parent / "fixtures"
    Path(work).mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, TZ="Asia/Taipei")
    with tempfile.TemporaryDirectory(dir=work) as scratch:
        out = Path(scratch) / "out"
        subprocess.run([harness, str(fixtures), str(out), "core-leaf"], env=env, check=True)
        verify_output(out / "core-leaf.actual.json")
        # An implemented selector must still SKIP without a macOS oracle.
        result = subprocess.run([sys.executable, str(here / "run_selector.py"), harness,
                                 str(fixtures), str(Path(scratch) / "missing-golden"),
                                 str(Path(scratch) / "runner"), "core-leaf"],
                                env=env, capture_output=True, text=True, check=False)
        assert result.returncode == 77 and "golden missing" in result.stdout and "not ported" not in result.stdout
        bad = Path(scratch) / "bad"
        bad.mkdir()
        for data in (None, "{", "[]", "{}"):
            path = bad / "core-leaf.json"
            if data is not None:
                path.write_text(data, encoding="utf-8")
            result = subprocess.run([harness, str(bad), str(out), "core-leaf"], env=env, capture_output=True, check=False)
            assert result.returncode == 1
    print("core-leaf self-test OK (branches, integer precision, fixture errors, missing oracle skip)")


if __name__ == "__main__":
    main()
