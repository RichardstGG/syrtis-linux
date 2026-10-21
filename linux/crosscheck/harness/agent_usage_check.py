"""Structural oracle checks, with separate C++ regression checks (no goldens)."""

import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def verify_structure(path, fixture_path=None):
    """Used by Swift: JSON validity, one output per case, matching ids only."""
    def invalid_constant(value):
        raise ValueError(f"invalid JSON constant: {value}")

    out = json.loads(Path(path).read_text(encoding="utf-8"), parse_constant=invalid_constant)
    fixture_path = fixture_path or Path(__file__).resolve().parents[1] / "fixtures/agent-usage.json"
    fixture = json.loads(Path(fixture_path).read_text(encoding="utf-8"))
    assert isinstance(out, dict) and isinstance(out.get("cases"), list)
    for c in out["cases"]:
        assert isinstance(c, dict) and isinstance(c.get("id"), str)
        assert ("value" in c) != ("error" in c), c["id"]
    ids = [c["id"] for c in out["cases"]]
    assert ids == [c["id"] for c in fixture["cases"]]
    assert len(ids) == len(set(ids))
    return out["cases"]


def verify_cpp_output(path):
    """Linux-only assertions. Never import/call these as Swift expectations."""
    out = {c["id"]: c for c in verify_structure(path)}

    def error(id, kind, path=(), **extra):
        assert out[id]["error"] == dict(kind=kind, path=list(path), **extra), (id, out[id])

    for typ in ("HistoricalPace", "PaceStatus", "UsageWindow", "AgentIdentity", "CreditsSnapshot",
                "AgentUsageTransportDiagnostic", "AgentUsageSnapshot", "AgentUsagePayload",
                "QuotaProviderIds", "QuotaCurvePoint", "QuotaCurveCoverage", "QuotaCurve"):
        assert "value" in out[f"valid-{typ}"], out[f"valid-{typ}"]
        error(typ + "-null-root", "valueNotFound")
        error(typ + "-array-root", "typeMismatch")
    assert out["valid-AgentUsagePayload"]["value"]["publicationGeneration"] == 9007199254740993
    assert out["valid-QuotaCurve"]["value"]["generation"] == 9007199254740993
    assert out["point-int64-exact"]["value"]["sampledAt"] == 9007199254740993
    assert out["point-cycle-lower-bound"]["value"]["sampledAt"] == -9223372036854775808
    assert out["payload-configured-order"]["configuredClientIds"] == ["codex", "antigravity"]
    assert out["window-legacy"]["value"]["durationSeconds"] is None
    assert out["window-top-duration-ignored"]["value"]["durationSeconds"] == 18000
    assert out["window-subminute"]["value"]["durationSeconds"] == 59
    assert out["status-reason-accountScope"]["historyKey"] is None
    assert out["status-reason-history"]["historyKey"] == "synthetic-window"
    assert out["valid-AgentUsageSnapshot"]["value"]["historyAccountKey"] is None
    for id, c in out.items():
        if id.startswith("diagnostic-forgiving-"):
            assert "value" in c
        if id.startswith(("point-invalid-", "status-invalid-", "historical-invalid-")):
            assert c.get("error", {}).get("kind") == "dataCorrupted", c
    error("UsageWindow-missing-label", "keyNotFound", field="label")
    error("UsageWindow-null-label", "valueNotFound", ("label",))
    error("UsageWindow-wrong-label", "typeMismatch", ("label",))
    error("HistoricalPace-null-willLastToReset", "typeMismatch", ("willLastToReset",))
    error("CreditsSnapshot-null-unlimited", "typeMismatch", ("unlimited",))
    error("payload-key-not-found-nested", "keyNotFound", ("data", "agents", "Index 0", "windows", "Index 0"), field="label")
    error("payload-bad-nested-window", "valueNotFound", ("agents", "Index 0", "windows", "Index 0", "usedPercent"))
    error("payload-bad-subscription", "typeMismatch", ("opencodeSubscriptions", "Index 1"))
    error("window-validation-order", "typeMismatch", ("resetText",))
    error("window-fractional-int", "dataCorrupted", ("windowMinutes",))
    error("QuotaCurve-missing-activeResetAt", "dataCorrupted")
    error("QuotaCurvePoint-missing-isActiveGroup", "dataCorrupted")
    error("curve-point-bool-null-nested", "typeMismatch", ("points", "Index 0", "isActiveGroup"))
    for id in ("curve-empty", "curve-count-mismatch", "curve-first-mismatch", "curve-last-mismatch",
               "curve-unsorted", "curve-duplicate"):
        error(id, "dataCorrupted")
    assert out["curve-optional-null"]["value"] is None
    assert "value" in out["curve-equal-time-distinct-cycle"]
    for id in ("point-min-duration", "point-max-duration", "window-state-learningDuration",
               "window-state-learningHistory", "window-state-available", "window-state-unavailable"):
        assert "value" in out[id], out[id]
    expected = {
        "lengths": ["Spark · Session", "Spark · Weekly", "Unique"],
        "spans": ["Spark · 1h", "Spark · 1h 30m"],
        "same-rendered-length": ["Spark · 1", "Spark · 2"],
        "reset": ["Spark · 1h", "Spark · 2h"],
        "partial-duration-reset": ["Spark · 1h", "Spark · 2h"],
        "reset-rounding-collision": ["Spark · 1", "Spark · 2"],
        "partial-duration": ["Spark · 1", "Spark · 2"],
        "ordinal": ["Spark · 1", "Spark · 2", "Spark · 3"],
        "duration-collision-untouched": ["Spark · 1", "Spark · 2", "Spark · Session"],
        "reset-collision-untouched": ["Spark · 1", "Spark · 2", "Spark · 1h"],
        "ordinal-collision": ["Spark · 3", "Spark · 4", "Spark · 1", "Spark · 2"],
        "duplicate-card": ["Spark · 1", "Spark · 2"],
        "legacy-collision": ["Synthetic"],
        "invalid-reset": ["Spark · 1", "Spark · 2"],
        "canonical-labels": ["Café · 1", "Cafe\u0301 · 2"],
    }
    for id, labels in expected.items():
        value = out["qualify-" + id]["value"]
        assert [w["label"] for w in value["qualified"]] == labels, (id, value)
        assert [w["cardId"] for w in value["qualified"]] == [w["cardId"] for w in value["raw"]]
    for id, c in out.items():
        if id.startswith("qualify-"):
            labels = [w["label"] for w in c["value"]["qualified"]]
            assert len(labels) == len(set(labels))
    assert out["snapshot-adopt"]["card"]["historyReadAccountKey"] == "synthetic-account"
    assert out["snapshot-adopt"]["value"]["windows"][0]["durationSeconds"] == 18000
    assert out["window-replace-mismatched-duration"]["value"]["durationSeconds"] == 604800
    assert out["window-replace-legacy"]["value"]["durationSeconds"] == 18000
    assert out["snapshot-remove-identity"]["value"]["identity"] is None
    assert out["payload-replace-agents"]["configuredClientIds"] == ["claude"]


def main():
    harness, work = sys.argv[1:]
    Path(work).mkdir(parents=True, exist_ok=True)
    fixtures = Path(__file__).resolve().parents[1] / "fixtures"
    env = dict(os.environ, TZ="Asia/Taipei")
    with tempfile.TemporaryDirectory(dir=work) as scratch:
        scratch = Path(scratch)
        subprocess.run([harness, str(fixtures), str(scratch / "out"), "agent-usage"], env=env, check=True)
        verify_cpp_output(scratch / "out/agent-usage.actual.json")
        local = scratch / "fixtures"
        local.mkdir()
        fixture = json.loads((fixtures / "agent-usage.json").read_text())
        curve = copy.deepcopy(next(c["input"] for c in fixture["cases"] if c["id"] == "valid-QuotaCurve"))
        # Construct the ceiling locally instead of checking in megabytes of
        # repeated fixture data. This check does not produce an oracle golden.
        points = [dict(curve["points"][0], sampledAt=i, resetAt=i + 1, durationSeconds=1) for i in range(65537)]
        curve.update(points=points, coverage=dict(oldestSampledAt=0, newestSampledAt=65536, sampleCount=65537))
        (local / "agent-usage.json").write_text(json.dumps({"cases": [dict(id="ceiling", type="QuotaCurve", input=curve)]}))
        subprocess.run([harness, str(local), str(scratch / "out"), "agent-usage"], env=env, check=True)
        result = json.loads((scratch / "out/agent-usage.actual.json").read_text())["cases"][0]
        assert result["error"] == dict(kind="dataCorrupted", path=[])
        # The exact ceiling is admitted, without weakening any per-point checks.
        curve["points"].pop()
        curve["coverage"].update(newestSampledAt=65535, sampleCount=65536)
        (local / "agent-usage.json").write_text(json.dumps({"cases": [dict(id="at-ceiling", type="QuotaCurve", input=curve)]}))
        subprocess.run([harness, str(local), str(scratch / "out"), "agent-usage"], env=env, check=True)
        result = json.loads((scratch / "out/agent-usage.actual.json").read_text())["cases"][0]
        assert len(result["value"]["points"]) == 65536
        for bad in ({}, {"cases": {}}, {"cases": [{"id": "bad", "type": "unknown", "input": {}}]},
                    {"cases": [fixture["cases"][0], fixture["cases"][0]]}):
            (local / "agent-usage.json").write_text(json.dumps(bad))
            assert subprocess.run([harness, str(local), str(scratch / "out"), "agent-usage"], env=env, capture_output=True).returncode != 0
    print("agent-usage C++ self-test OK (decoding, paths, qualification, curve ceiling, fixture guards)")


if __name__ == "__main__":
    main()
