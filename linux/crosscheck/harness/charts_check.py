"""Synthetic model regression checks; independent of the macOS parity gate."""

import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def verify_output(path):
    rows = json.loads(Path(path).read_text(encoding="utf-8"))["cases"]
    fixture = json.loads((Path(__file__).resolve().parents[1] / "fixtures/charts.json").read_text())
    assert [c["id"] for c in rows] == [c["id"] for c in fixture["cases"]]
    out = {c["id"]: c for c in rows}
    mx, mn = 2**63 - 1, -(2**63)
    for c in fixture["cases"]:
        row = out[c["id"]]
        if c["op"] == "payload":
            assert "error" not in row, row
            # The production decoder preserves Rust's intentionally distinct
            # pre-aggregates. These are not reconstructed from chart stripes.
            assert row["value"]["meta"] == c["input"]["meta"]
            assert row["value"]["summary"] == c["input"]["summary"]
            assert row["value"]["years"] == c["input"]["years"]
            for source, actual in zip(c["input"]["contributions"], row["value"]["contributions"]):
                assert actual == dict(source, turnsByClient=source.get("turnsByClient"))
            for mode in ("model", "agent"):
                bars = row[mode]["bars"]
                assert len(bars) == (0 if c["id"] == "invalid-end" else 30)
                assert [b["date"] for b in bars] == sorted(b["date"] for b in bars)
                for bar in bars:
                    assert bar["isEmpty"] == (not bar["segments"])
                    assert [s["key"] for s in bar["segments"]] == sorted(s["key"] for s in bar["segments"])
                for name, metric in (("costLegend", "cost"), ("tokenLegend", "tokens")):
                    values = [s[metric] for s in row[mode][name]]
                    assert values == sorted(values, reverse=True)
            assert all(t["empty"] is None for t in row["turns"])
        if c["id"].startswith("decode-"):
            actual = row["value"]
            # TraceBucket maps snake_case wire spelling to a camelCase model.
            expected = dict(c["input"])
            if c["type"] == "TraceBucket":
                expected["tokensPerMin"] = expected.pop("tokens_per_min")
            if c["type"] == "Contribution":
                expected["turnsByClient"] = expected.get("turnsByClient")
            if c["type"] == "UsagePayload":
                expected["contributions"] = [dict(d, turnsByClient=d.get("turnsByClient"))
                                             for d in expected["contributions"]]
            assert actual == expected, c["id"]

    filtered = out["filtered"]
    stats = filtered["stats"]
    assert (stats["totalTokens"], stats["totalCost"], stats["activeDays"]) == (13, 4.25, 3)
    assert abs(stats["averagePerDay"] - 4.25 / 3) < 1e-9
    assert stats["maxTokens"] == 13 and stats["bestDay"] == {"date": "2024-03-01", "cost": 3}
    assert stats["dateRange"] == {"start": "2024-02-28", "end": "2024-03-01"}
    assert stats["streaks"] == {"longest": 2, "current": 0}
    assert stats["perDayMap"]["2024-02-29"]["isActive"] is True  # messages only
    assert stats["perDayMap"]["2024-03-01"]["isActive"] is False  # cost only
    assert filtered["turns"][0] == {"selected": 2, "empty": None}
    assert filtered["model"]["bars"][-1]["date"] == "2024-03-01"
    assert filtered["model"]["tokenLegend"][0] == dict(
        key="claude-sonnet-4", label="claude-sonnet-4", color="#da7756", tokens=13, cost=4.25)
    assert filtered["agent"]["tokenLegend"][0]["label"] == "Claude"
    assert out["all-fast"]["tray"] == dict(todayTokens=101, todayCost=9.5,
        totalTokens=999, totalCost=99.5, todayTopClient="codex")
    assert out["all-fast"]["stats"]["dateRange"]["start"] == "2024-02-27"
    assert out["only-claude"]["tray"] == filtered["tray"]
    for name in ("only-empty", "only-hidden"):
        assert out[name]["tray"] == dict(todayTokens=0, todayCost=0, totalTokens=0,
                                         totalCost=0, todayTopClient=None)
    assert out["none-selected"]["stats"]["bestDay"] is None
    assert out["none-selected"]["stats"]["presentClients"] == ["claude", "codex"]
    assert out["none-selected"]["turns"][0]["selected"] is None
    assert out["empty-fallback"]["model"]["bars"][-1]["date"] == "2024-04-01"
    assert out["empty-range"]["model"]["bars"][-1]["date"] == "2024-02-29"
    assert not out["invalid-end"]["model"]["bars"]
    assert out["message-only"]["hasVisibleActivity"] is True
    assert all(b["isEmpty"] for b in out["message-only"]["model"]["bars"])
    assert out["cost-only"]["model"]["bars"][-1]["totalCost"] == 2
    assert out["cost-only"]["stats"]["streaks"] == {"longest": 0, "current": 0}
    saturated = out["saturated"]
    assert saturated["stats"]["totalTokens"] == saturated["tray"]["totalTokens"] == mx
    assert saturated["model"]["tokenLegend"][0]["tokens"] == mx
    assert saturated["turns"][0]["selected"] == mx
    assert out["negative"]["stats"]["totalTokens"] == mn
    assert all(b["isEmpty"] for b in out["negative"]["model"]["bars"])
    assert out["unregistered-only"]["tray"]["todayTopClient"] == "cc-mirror/test"
    assert out["unregistered-only"]["visibleYears"] == ["2023"]
    assert out["unregistered-only"]["model"]["tokenLegend"][0]["key"] == "unknown"
    assert out["top-tie"]["tray"]["todayTopClient"] == "a-agent"
    assert out["top-cost-tie"]["tray"]["todayTopClient"] == "b-agent"
    duplicate = out["duplicate-dates"]
    assert duplicate["stats"]["totalTokens"] == 12
    assert duplicate["model"]["bars"][-1]["totalTokens"] == 7
    assert duplicate["stats"]["perDayMap"]["2024-02-28"]["tokens"] == 7
    assert out["window-truncation"]["model"]["tokenLegend"][0]["tokens"] == 1
    assert out["best-first-tie"]["stats"]["bestDay"]["date"] == "2024-02-28"
    assert out["wide-intensity"]["stats"]["perDay"][0]["intensity"] == 2147483648

    overview = out["overview-default"]
    assert overview["allCases"] == ["quotaSummary", "chart", "limits", "trace", "models", "streaks"]
    assert overview["visible"] == overview["allCases"]
    assert overview["labels"]["quotaSummary"] == "Quota summary"
    assert overview["hiddenKey"] == "tokenbar.overview.hidden" and "chart" not in overview["toggleable"]
    assert out["overview-anchor"]["visible"] == ["chart"]
    assert out["overview-whitespace"]["visible"] == ["chart", "limits"]
    assert out["overview-case"]["visible"] == ["quotaSummary", "chart", "limits", "models", "streaks"]
    colors = out["colors"]
    assert colors["shades"] == ["#000000", "#1c1c1c", "#ababab", "#bdd8f3", "#12345g", "bad", "#ffffff", "#e6a48e"]
    assert out["shade-signed"]["shades"] == ["#ffffffffffff", "#01ffff", "# ff ff"]
    assert [v["inferred"] for v in colors["lookups"][:19]] == [
        "anthropic", *(["openai"] * 8), "google", "deepseek", "xai", "meta", "mistral", "mistral",
        "cursor", "cursor", "cursor", "unknown"]
    assert [v["color"] for v in colors["lookups"][-11:-2]] == [
        "#3b82f6", "#5190f7", "#669ef8", "#7cabf9", "#91b9fa", "#a9c8fb", "#bed6fc", "#bed6fc", "#bed6fc"]
    assert colors["lookups"][1]["reportColor"] == "#3b82f6"
    assert colors["lookups"][-1]["color"] == colors["lookups"][-2]["color"]
    assert [v["color"] for v in out["colors-ties"]["lookups"]] == ["#3b82f6", "#5190f7"]
    trace = out["trace-filter"]
    assert trace["totalRate"] == 3.5
    assert abs(out["trace-all"]["totalRate"] - 5.1) < 1e-9
    assert [r["client"] for r in trace["collapsed"]] == ["codex", "claude-code", "other", "claude"]
    assert trace["collapsed"][1] == dict(client="claude-code", agent="a, z", model="claude-sonnet",
                                        tokens=9, messages=2, tokensPerMin=1.5)
    assert trace["collapsed"][2]["model"] == "unknown"
    assert out["trace-empty"]["collapsed"] == [] and out["trace-empty"]["totalRate"] == 0
    assert [r["client"] for r in out["trace-saturated"]["collapsed"]] == ["x", "y"]
    assert out["trace-saturated"]["collapsed"][0]["tokens"] == mx
    assert out["trace-negative"]["collapsed"][0]["tokens"] == mn
    for name, expected in (("streak-cost", (2, 0)), ("streak-current", (2, 2)),
                           ("streak-invalid", (0, 0)), ("streak-reversed", (0, 0))):
        assert (out[name]["longest"], out[name]["current"]) == expected
    assert out["token-saturate-max"]["total"] == mx - 1
    assert out["token-saturate-min"]["total"] == mn + 1
    assert out["token-exact"]["total"] == 9007199254740993
    for name in ("turns-missing", "turns-null"):
        assert out[name]["value"]["turnsByClient"] is None
    assert out["turns-empty"]["value"]["turnsByClient"] == {}

    def error(name, kind, path=(), **extra):
        assert out[name]["error"] == dict(kind=kind, path=list(path), **extra), out[name]

    for name, field in (("token-missing", "reasoning"), ("client-missing", "messages"),
                        ("trace-camel-spelling", "tokens_per_min"), ("rate-snake-spelling", "tokensPerMin")):
        error(name, "keyNotFound", field=field)
    for name, path in (("token-bool", ["input"]), ("turns-wrong-element", ["turnsByClient", "claude"]),
                       ("turns-array", ["turnsByClient"]), ("trace-string", ["tokens_per_min"]),
                       ("payload-client-element", ["summary", "clients", "Index 1"]), ("scalar", [])):
        error(name, "typeMismatch", path)
    error("token-fraction", "dataCorrupted")
    error("clients-null", "valueNotFound", ["clients"])
    error("payload-missing-end", "keyNotFound", ["meta", "dateRange"], field="end")
    error("malformed", "dataCorrupted")


def main():
    harness, work = sys.argv[1:]
    Path(work).mkdir(parents=True, exist_ok=True)
    fixtures = Path(__file__).resolve().parents[1] / "fixtures"
    env = dict(os.environ, TZ="Asia/Taipei")
    with tempfile.TemporaryDirectory(dir=work) as scratch:
        scratch = Path(scratch)
        out = scratch / "out"
        subprocess.run([harness, str(fixtures), str(out), "charts"], check=True, env=env)
        verify_output(out / "charts.actual.json")
        assert {p.name for p in out.iterdir()} == {"charts.actual.json"}
        # Exercise the shared conversion rule without changing the fixtures
        # paired with the immutable macOS golden. These are C++ regressions;
        # only token-fraction has already been qualified by the Swift oracle.
        source = json.loads((fixtures / "charts.json").read_text())["cases"]
        numeric = scratch / "numeric"
        numeric.mkdir()
        cases = []
        token_case = next(c for c in source if c["id"] == "token-fraction")
        client_case = next(c for c in source if c["id"] == "decode-ContributionClient")
        contribution_case = next(c for c in source if c["id"] == "decode-Contribution")
        payload_case = next(c for c in source if c["id"] == "decode-UsagePayload")
        color_case = next(c for c in source if c["id"] == "colors")
        for value in (1.5, 2**63, -(2**64)):
            for template in (token_case, client_case, contribution_case, payload_case):
                c = copy.deepcopy(template)
                c["id"] = f"signed-{len(cases)}"
                if c["type"] == "TokenBreakdown":
                    c["input"]["input"] = value
                elif c["type"] == "ContributionClient":
                    c["input"]["tokens"]["input"] = value
                elif c["type"] == "Contribution":
                    c["input"]["turnsByClient"] = {"claude": value}
                else:
                    c["input"]["contributions"][0]["clients"][0]["tokens"]["input"] = value
                cases.append(c)
        for value in (1.5, -1, 2**64):
            c = copy.deepcopy(color_case)
            c["id"] = f"unsigned-{len(cases)}"
            c["report"]["pricingUpdatedAt"] = value
            cases.append(c)
        (numeric / "charts.json").write_text(json.dumps({"cases": cases}), encoding="utf-8")
        subprocess.run([harness, str(numeric), str(out), "charts"], check=True, env=env)
        results = json.loads((out / "charts.actual.json").read_text())["cases"]
        assert [c["id"] for c in results] == [c["id"] for c in cases]
        for c in results:
            assert c["error"] == {"kind": "dataCorrupted", "path": []}, c
        bad = scratch / "fixtures"
        bad.mkdir()
        for value in (None, {}, {"cases": {}}, {"cases": [{}]},
                      {"cases": [{"id": "bad", "op": "unknown"}]}):
            (bad / "charts.json").write_text(json.dumps(value))
            result = subprocess.run([harness, str(bad), str(out), "charts"], env=env,
                                    capture_output=True, check=False)
            assert result.returncode == 1
        (bad / "charts.json").unlink()
        assert subprocess.run([harness, str(bad), str(out), "charts"], env=env,
                              capture_output=True, check=False).returncode == 1
    print("charts self-test OK (models, filters, saturation, wire errors, fixture I/O)")


if __name__ == "__main__":
    main()
