"""Synthetic usage-pace contract checks; these are not oracle goldens."""

import copy
import json
import math
import subprocess


def check_usage_pace(harness, scratch):
    fixtures = scratch / "pace-fixtures"
    fixtures.mkdir()
    out = scratch / "pace-output"
    now = "2026-07-10T12:00:00Z"
    window = {
        "cardId": "synthetic-card", "label": "Synthetic window",
        "usedPercent": 60, "remainingPercent": 40,
        "resetsAt": "2026-07-10T12:50:00Z", "windowMinutes": 100,
        "paceStatus": {"state": "learningHistory", "windowKey": "synthetic-window",
                       "durationSeconds": 6000, "durationSource": "provider", "completeCycles": 0},
    }
    historical = {"expectedUsedPercent": 40, "etaSeconds": 123.5,
                  "willLastToReset": False, "runOutProbability": .3}
    cases = []
    expected = {}

    def add(name, value, *, result=None, **metadata):
        cases.append(dict(name=name, kind="compute", mode="linear", now=now,
                          window=copy.deepcopy(value), **metadata))
        expected[name] = result

    linear = dict(stage="ahead", deltaPercent=10, expectedUsedPercent=50,
                  actualUsedPercent=60, etaSeconds=2000, willLastToReset=False,
                  label="10% in deficit", etaText="Projected empty in 33m")
    add("linear", window, result=linear)
    for source in ("provider", "contract", "observed"):
        value = copy.deepcopy(window)
        value["paceStatus"]["durationSource"] = source
        add(f"duration-source-{source}", value, result=linear)
    value = copy.deepcopy(window)
    value["paceStatus"]["state"] = "available"
    value["historicalPace"] = historical
    add("available-linear", value, result=linear)
    cases.append(dict(name="historical", kind="compute", mode="historical", now=now, window=value))
    expected["historical"] = dict(linear, stage="farAhead", deltaPercent=20, expectedUsedPercent=40,
                                  etaSeconds=123.5, label="20% in deficit", etaText="Projected empty in 2m")
    cases.append(dict(name="risk", kind="runOutRisk", window=value))
    expected["risk"] = "≈ 30% run-out risk"
    for probability, text in ((0, None), (.004, None), (.005, "≈ 1% run-out risk"),
                              (1, "≈ 100% run-out risk")):
        risk = copy.deepcopy(value)
        risk["historicalPace"]["runOutProbability"] = probability
        cases.append(dict(name=f"risk-{probability}", kind="runOutRisk", window=risk))
        expected[f"risk-{probability}"] = text

    for state in ("learningDuration", "unavailable"):
        value = copy.deepcopy(window)
        value.pop("windowMinutes")
        value["paceStatus"] = dict(state=state, windowKey="synthetic-window", completeCycles=0)
        if state == "unavailable":
            value["paceStatus"]["reason"] = "history"
        add(state, value)
    value = copy.deepcopy(window)
    value.pop("paceStatus")
    add("legacy-ignores-minutes", value)
    value["historicalExpectedPercent"] = 150
    value["runOutProbability"] = 1.5
    add("legacy-ignores-scalars", value)

    # Strict production-decoder failures must be rejected before mode/now.
    mutations = [
        ("percentage-range", "usedPercent", -1), ("percentage-bool", "usedPercent", True),
        ("percentage-sum", "remainingPercent", 39), ("percentage-string", "usedPercent", "60"),
        ("label-type", "label", 7), ("card-null", "cardId", None), ("card-blank", "cardId", " \n"),
        ("status-null", "paceStatus", None), ("window-minutes-fraction", "windowMinutes", 100.5),
        ("window-minutes-mismatch", "windowMinutes", 99), ("reset-type", "resetsAt", False),
        ("reset-text-type", "resetText", []), ("model-scope-type", "modelScope", 1),
        ("historical-type", "historicalPace", []),
    ]
    for name, key, invalid in mutations:
        value = copy.deepcopy(window)
        value[key] = invalid
        add(name, value, result={"rejected": True})
    for key, invalid in (("state", "legacyMissing"), ("state", "future"), ("completeCycles", -1),
                         ("completeCycles", .5), ("completeCycles", True), ("completeCycles", 2**63),
                         ("durationSeconds", 0), ("durationSeconds", 400 * 86400 + 1),
                         ("durationSeconds", 6000.5), ("durationSource", "guess"),
                         ("durationSource", None), ("windowKey", ""), ("windowKey", None),
                         ("reason", "history")):
        value = copy.deepcopy(window)
        value["paceStatus"][key] = invalid
        add(f"bad-status-{key}-{invalid}", value, result={"rejected": True})
    for key, invalid in (("expectedUsedPercent", 101), ("expectedUsedPercent", None),
                         ("etaSeconds", -1), ("etaSeconds", None), ("willLastToReset", True),
                         ("willLastToReset", 0), ("runOutProbability", 1.5)):
        value = copy.deepcopy(window)
        value["paceStatus"]["state"] = "available"
        value["historicalPace"] = dict(historical, **{key: invalid})
        add(f"bad-historical-{key}-{invalid}", value, result={"rejected": True})
    for key in ("label", "usedPercent", "remainingPercent", "cardId", "windowMinutes"):
        value = copy.deepcopy(window)
        value.pop(key)
        add(f"missing-{key}", value, result={"rejected": True})
    for key in ("state", "completeCycles", "durationSeconds", "windowKey"):
        value = copy.deepcopy(window)
        value["paceStatus"].pop(key)
        add(f"missing-status-{key}", value, result={"rejected": True})

    for key, invalid, message in (("mode", "future", "unknown pace mode future"),
                                  ("mode", None, "unknown pace mode nil"),
                                  ("now", "2026-07-10", "unparseable now 2026-07-10"),
                                  ("now", None, "unparseable now nil"),
                                  ("kind", "future", "unknown kind future")):
        name = f"metadata-{key}-{invalid}"
        cases.append(dict(name=name, kind="compute", mode="linear", now=now, window=window))
        cases[-1][key] = invalid
        expected[name] = {"error": message}
    cases.append({"window": window})
    expected[f"invalid-case-{len(cases) - 1}"] = {"error": "invalid case metadata"}
    (fixtures / "usage-pace.json").write_text(json.dumps({"cases": cases}), encoding="utf-8")
    subprocess.run([harness, str(fixtures), str(out), "usage-pace"], check=True)
    actual = json.loads((out / "usage-pace.actual.json").read_text(encoding="utf-8"))
    assert actual.keys() == expected.keys(), "usage-pace output case set changed"
    for name, wanted in expected.items():
        result = actual[name]
        if isinstance(wanted, dict):
            assert isinstance(result, dict) and result.keys() == wanted.keys(), (name, result, wanted)
            for key, field in wanted.items():
                if isinstance(field, (int, float)) and not isinstance(field, bool):
                    assert type(result[key]) in (int, float) and math.isclose(
                        result[key], field, rel_tol=0, abs_tol=1e-9), (name, key, result, wanted)
                else:
                    assert type(result[key]) is type(field) and result[key] == field, (name, key, result, wanted)
        else:
            assert type(result) is type(wanted) and result == wanted, (name, result, wanted)
    # Bad fixture roots and missing files fail; no fabricated actual output.
    for root in ({}, {"cases": [1]}, {"cases": None}):
        (fixtures / "usage-pace.json").write_text(json.dumps(root), encoding="utf-8")
        assert subprocess.run([harness, str(fixtures), str(out), "usage-pace"],
                              capture_output=True).returncode != 0
    (fixtures / "usage-pace.json").unlink()
    assert subprocess.run([harness, str(fixtures), str(out), "usage-pace"], capture_output=True).returncode != 0
    print(f"usage-pace synthetic harness checks OK ({len(expected)} cases)")
