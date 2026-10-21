"""Structural oracle checks and separate C++ decoder regression assertions.

These checks never write oracle goldens. Swift/C++ parity remains a separate
gate using run_selector.py and diff.py.
"""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def verify_structure(path, fixture_path=None):
    """Check JSON and case coverage only; shipping Swift owns the semantics."""
    def invalid_constant(value):
        raise ValueError(f"invalid JSON constant: {value}")

    output = json.loads(Path(path).read_text(encoding="utf-8"), parse_constant=invalid_constant)
    fixture_path = fixture_path or Path(__file__).resolve().parents[1] / "fixtures/reports.json"
    fixture = json.loads(Path(fixture_path).read_text(encoding="utf-8"))
    assert isinstance(output, dict) and isinstance(output.get("cases"), list)
    cases = output["cases"]
    for c in cases:
        assert isinstance(c, dict) and isinstance(c.get("id"), str)
        assert ("value" in c) != ("error" in c), c["id"]
        if "error" in c:
            assert isinstance(c["error"], dict) and isinstance(c["error"].get("kind"), str)
        if "smokeSummary" in c:
            assert isinstance(c["smokeSummary"], str)
    ids = [c["id"] for c in cases]
    assert ids == [c["id"] for c in fixture["cases"]]
    assert len(ids) == len(set(ids))
    return cases


def verify_cpp_output(path):
    """C++-only regressions; never invoked before oracle golden generation."""
    cases = verify_structure(path)
    fixture = json.loads((Path(__file__).resolve().parents[1] / "fixtures/reports.json").read_text())
    out = {c["id"]: c for c in cases}
    for c in fixture["cases"]:
        if not c["id"].startswith("valid-"):
            continue
        # Every payload's fields survive decoding; unknown fields are tested
        # separately below. This catches missing fields in harness serializers.
        expected = c["input"] if c["mode"] == "bare" else c["input"]["data"]
        assert out[c["id"]]["value"] == expected
    for name in ("AgentsReport", "HourlyReport", "ModelReport"):
        assert out[f"empty-{name}"]["value"]["entries"] == []
        value = out[f"exact-{name}"]["value"]
        entry = value["entries"][0]
        assert (entry["input"], entry["output"], entry["reasoning"]) == (
            9007199254740993, 9223372036854775807, -9223372036854775808)
        assert entry["total"] == -7 and value["totalCost"] == 99.5  # Never re-aggregate.
    for name in ("model-legacy", "model-wrong-throughput-spelling", "model-null-optionals"):
        model = out[name]["value"]
        assert model["pricingUpdatedAt"] is None
        assert model["entries"][0]["costEstimate"] is None
        assert model["entries"][0]["msPer1kTokens"] is None
        assert "unknownAdditive" not in model and "msPer1KTokens" not in model["entries"][0]
    for name, fields in {
        "ProbeResult": ["messages", "err"], "AntigravityAutoCaptureResult": ["key", "label"],
        "CursorSyncConfigResult": ["dir"], "CursorSyncStatus": ["lastSuccessMs", "reason"],
    }.items():
        assert all(out[f"missing-optionals-{name}"]["value"][key] is None for key in fields)
    assert out["valid-FilterParityProbe"]["smokeSummary"] == (
        "hourly=MATCH entriesΔ=0 tokensΔ=0 messagesΔ=0 costΔ=0.00; agents=SOURCE_CHANGED / SKIP")
    for status, label in (("match", "MATCH"), ("mismatch", "MISMATCH")):
        assert out[f"parity-{status}"]["smokeSummary"] == (
            f"hourly={label} entriesΔ=-1 tokensΔ=-13 messagesΔ=-2 costΔ=-1.12; agents={label}")
    for status, label in (("sourceChanged", "SOURCE_CHANGED / SKIP"), ("tokenUnavailable", "TOKEN_UNAVAILABLE / SKIP")):
        assert out[f"parity-{status}"]["smokeSummary"] == f"hourly={label}; agents={label}"

    def error(name, kind, path=(), **extra):
        assert out[name]["error"] == dict(kind=kind, path=list(path), **extra), (name, out[name])

    row = ("data", "entries", "Index 0")
    for field in ("agent", "clients", "input", "output", "cacheRead", "cacheWrite", "reasoning", "total", "cost", "messages"):
        error(f"agent-missing-{field}", "keyNotFound", row, field=field)
    error("agent-null", "valueNotFound", (*row, "agent"))
    error("hourly-missing-turns", "keyNotFound", row, field="turnCount")
    error("model-missing-provider", "keyNotFound", row, field="provider")
    error("scan-missing-reason", "keyNotFound", ("data", "unreadable", "Index 0"), field="reason")
    error("parity-missing-count", "keyNotFound", ("data",), field="presentClientCount")
    for name, path in {
        "agent-input-string": (*row, "input"), "agent-cost-bool": (*row, "cost"),
        "agent-clients-wrong-element": (*row, "clients", "Index 1"),
        "model-throughput-string": (*row, "msPer1kTokens"), "model-estimate-bool": (*row, "costEstimate"),
        "model-pricing-string": ("data", "pricingUpdatedAt"), "parity-invalid-aggregate": ("data", "hourly", "delta"),
        "cursor-time-string": ("data", "lastSuccessMs"), "capture-label-number": ("data", "label"),
        "binding-not-bool": ("data", "bound"), "bridge-malformed-error": ("err",),
        "success-malformed-error": ("err",), "string-ok": ("ok",), "array-envelope": (), "array-data": ("data",),
    }.items():
        error(name, "typeMismatch", path)
    error("hourly-models-null", "valueNotFound", (*row, "models"))
    for status in ("MATCH", "source_changed", "futureStatus"):
        error(f"parity-invalid-{status}", "dataCorrupted", ("data", "hourly", "status"))
    for name, message in {
        "bridge-message": "synthetic_failure", "bridge-optional": "synthetic_failure",
        "bridge-valid-data": "synthetic_failure", "bridge-unknown": "unknown",
        "bridge-null-error": "unknown", "bridge-empty-error": "",
    }.items():
        assert out[name]["error"] == dict(kind="bridge", message=message)
    error("bridge-malformed-data", "keyNotFound", ("data",), field="entries")
    for name in ("missing-data", "null-data"):
        error(name, "dataCorrupted", description="successful FFI envelope is missing data")
    error("optional-missing-data", "dataCorrupted", description="successful FFI envelope is missing the data key")
    assert out["optional-null-data"]["value"] is None
    assert out["optional-value"]["value"] == out["valid-AgentsReport"]["value"]
    assert out["success-null-error"]["value"] == out["valid-AgentsReport"]["value"]
    error("missing-ok", "keyNotFound", field="ok")
    error("null-ok", "typeMismatch", ("ok",))
    error("null-envelope", "valueNotFound")
    for name in ("malformed-json", "trailing-json", "empty-json"):
        error(name, "dataCorrupted")

    # Keyed Bool.decode checks type directly; String/number unwrap and
    # keyed/unkeyed container creation check null first. decodeIfPresent
    # accepts missing/null before checking the wrapped type. These C++-only
    # assertions follow Foundation's JSONDecoder, not a uniform null rule.
    # https://github.com/swiftlang/swift-foundation/blob/main/Sources/FoundationEssentials/JSON/JSONDecoder.swift
    boolean_fields = {
        ("ProbeResult", "ok"), ("AntigravityBindingResult", "bound"), ("AntigravityRemoved", "removed"),
        ("CursorSyncConfigResult", "enabled"), ("CursorSyncConfigResult", "cliTakeoverConfirmed"),
        ("CursorSyncConfigResult", "cleanupFailed"),
    }
    optional_fields = {
        ("ModelReportEntry", "costEstimate"), ("ModelReport", "pricingUpdatedAt"),
        ("CursorSyncStatus", "reason"), ("CursorSyncStatus", "lastSuccessMs"), ("FilterParityReport", "delta"),
    }
    for c in fixture["cases"]:
        if not c["id"].startswith("field-"):
            continue
        _, typ, field, variant = c["id"].split("-")
        parent = () if c["mode"] == "bare" else ("data",)
        if (typ, field) in optional_fields and variant != "wrong":
            assert out[c["id"]]["value"][field] is None
        elif variant == "missing":
            error(c["id"], "keyNotFound", parent, field=field)
        else:
            kind = "valueNotFound" if variant == "null" and (typ, field) not in boolean_fields else "typeMismatch"
            error(c["id"], kind, (*parent, field))
    error("null-string-array-element", "valueNotFound", ("data", "clients", "Index 0"))
    error("null-object-array-element", "valueNotFound", ("data", "entries", "Index 0"))


def main():
    harness, work = sys.argv[1:]
    here = Path(__file__).resolve().parent
    fixtures = here.parent / "fixtures"
    Path(work).mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, TZ="Asia/Taipei")
    with tempfile.TemporaryDirectory(dir=work) as scratch:
        out = Path(scratch) / "out"
        subprocess.run([harness, str(fixtures), str(out), "reports"], env=env, check=True)
        verify_cpp_output(out / "reports.actual.json")
        # A semantic difference must survive oracle preflight and reach diff.py.
        output = json.loads((out / "reports.actual.json").read_text())
        for c in output["cases"]:
            if c["id"] == "null-ok":
                c["error"]["kind"] = "futureOracleClassification"
        structural = Path(scratch) / "structure.json"
        structural.write_text(json.dumps(output), encoding="utf-8")
        verify_structure(structural)
        for cases in (output["cases"][:-1], output["cases"] + [output["cases"][0]], list(reversed(output["cases"]))):
            structural.write_text(json.dumps({"cases": cases}), encoding="utf-8")
            try:
                verify_structure(structural)
            except AssertionError:
                pass
            else:
                raise AssertionError("structure check accepted missing, duplicate, or reordered ids")
        structural.write_text("NaN", encoding="utf-8")
        try:
            verify_structure(structural)
        except ValueError:
            pass
        else:
            raise AssertionError("structure check accepted invalid JSON")
        result = subprocess.run([sys.executable, str(here / "run_selector.py"), harness,
                                 str(fixtures), str(Path(scratch) / "missing-golden"),
                                 str(Path(scratch) / "runner"), "reports"], env=env,
                                capture_output=True, text=True, check=False)
        assert result.returncode == 77 and "golden missing" in result.stdout and "not ported" not in result.stdout
        bad = Path(scratch) / "bad"
        bad.mkdir()
        for data in (None, "{", "[]", "{}", '{"cases": [1]}', '{"cases": [{"id":"x","type":"unknown","input":{}}]}'):
            if data is not None:
                (bad / "reports.json").write_text(data, encoding="utf-8")
            result = subprocess.run([harness, str(bad), str(out), "reports"], env=env,
                                    capture_output=True, check=False)
            assert result.returncode == 1
    print("reports self-test OK (structure, decoder errors, exact Int64, missing oracle SKIP)")


if __name__ == "__main__":
    main()
