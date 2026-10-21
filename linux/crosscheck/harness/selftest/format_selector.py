"""Synthetic Format contract checks, independent of the macOS oracle goldens."""

import copy
import json
from pathlib import Path
import subprocess


def check_format(harness, scratch):
    fixtures = scratch / "format-fixtures"
    fixtures.mkdir()
    out = scratch / "format-output"
    reference = Path(__file__).resolve().parents[2] / "fixtures/format.json"
    original = json.loads(reference.read_text(encoding="utf-8"))

    def run(payload, expected=None, exit_code=0):
        (fixtures / "format.json").write_text(json.dumps(payload), encoding="utf-8")
        result = subprocess.run([harness, str(fixtures), str(out), "format"],
                                capture_output=True, text=True, check=False)
        if result.returncode != exit_code:
            raise RuntimeError(f"format: unexpected exit {result.returncode}: {result.stderr}")
        if exit_code:
            return
        actual = json.loads((out / "format.actual.json").read_text(encoding="utf-8"))
        if expected is not None and actual != expected:
            raise RuntimeError(f"format mismatch: expected {expected}, got {actual}")
        return actual

    actual = run(original)
    if len(actual) != 74:
        raise RuntimeError("format fixture case count changed")
    # These assertions cover the seams that a plausible formatter often gets
    # wrong: binary rounding (not decimal pre-rounding), exact Int64 output,
    # local midnight, and selecting rather than summing duplicate day totals.
    for key, expected in {
        "compact-1050-half-to-even": "1.1K",
        "compact-1250-half-to-even": "1.2K",
        "compact-1350-half-to-even": "1.4K",
        "compact-999999-promotes-to-1M": "1M",
        "compact-999999999-promotes-to-1B": "1B",
        "compact-int64-max": "9223372037B",
        "exact-int64-max": "9,223,372,036,854,775,807",
        "usd-0.005-binary-nudge": "$0.01",
        "usd-0.015": "$0.01",
        "usd-0.025-half-to-even": "$0.03",
        "usd-0.125-half-to-even": "$0.12",
        "usd-three-decimals": "$7449.28",
        "usd-negative-sign-placement": "$-1.00",
        "todaytokens-duplicate-date-takes-last": 4200,
        "todaycost-duplicate-date-takes-last": 1.23,
        "todaytokens-int64-max-exact-roundtrip": 2**63 - 1,
        "todaytokens-last-local-second": 4200,
        "todaytokens-utc-crosses-local-midnight": 0,
        "duration-29s-now": "now",
        "duration-30s-rounds-to-1m": "1m",
    }.items():
        if actual.get(key) != expected:
            raise RuntimeError(f"{key}: expected {expected!r}, got {actual.get(key)!r}")

    payload = copy.deepcopy(original)
    payload["cases"] = []
    expected = {}

    def case(fn, arg=None, result=None, **metadata):
        name = f"synthetic-{len(payload['cases'])}-{fn}"
        payload["cases"].append(dict(name=name, fn=fn, arg=arg, **metadata))
        expected[name] = result

    for count, text in ((999499, "999K"), (999500, "1M"), (999499999, "999M"),
                        (999500000, "1B"), (-2**63, "-9223372036854775808")):
        case("compactTokens", count, text)
    case("exactTokens", -2**63, "-9,223,372,036,854,775,808")
    case("compactTokens", 1234.75, "1.2K")  # Arg.asInt64 truncates Double.
    case("exactTokens", "1234", "0")  # String arguments are not coerced.
    case("usd", -0.0, "$0.00")  # Swift Arg decodes integral numbers as Int64 first.
    case("usd", -0.001, "$-0.00")
    case("usd", 0.375, "$0.38")
    for iso, text in (("2026-13-01", "2026-13-01"), ("2026-00-01", "2026-00-01"),
                      ("2026-02-31", "Feb 31"), ("2026--06-10", "Jun 10"),
                      ("2026-nope-06-10", "Jun 10"), (" 2026-06-10", " 2026-06-10")):
        case("monthDay", iso, text)
    case("mmdd", "2026--6-5", "6/5")
    case("mmdd", "year-month-day", "month/day")
    case("mmdd", "2026-06-10-extra", "2026-06-10-extra")
    for fn in ("todayKey", "todayTokens", "todayCost", "todayTopClient", "relativeTime"):
        case(fn, result={"error": f"{fn} needs now"})
        case(fn, result={"error": f"{fn} needs now"}, now="invalid")
    case("unknown", result={"error": "unknown fn unknown"})
    now = "2026-07-10T12:00:00Z"
    for delta, text in ((59, "just now"), (60, "1m ago"), (3599, "59m ago"),
                        (3600, "1h ago"), (86399, "23h ago"), (86400, "1d ago"),
                        (-1, "just now")):
        case("relativeTime", 1783684800 - delta, text, now=now)
    case("relativeTime", 1783684741, "just now", now="2026-07-10T12:00:00.999Z")
    case("todayKey", result="2026-07-11", now="2026-07-10T16:00:00Z")
    case("todayKey", result="2026-07-10", now="2026-07-10T15:59:59Z")
    case("todayTopClient", result=None, now=now)
    case("paceDurationText", -30, "now")
    case("paceDurationText", 86370, "1d")
    run(payload, expected)

    def stripe(client, tokens, cost, **lanes):
        return dict(client=client, modelId="synthetic-model", providerId="synthetic-provider",
                    tokens=dict(input=tokens, output=lanes.get("output", 0),
                                cacheRead=0, cacheWrite=0, reasoning=0), cost=cost, messages=1)

    # Each scenario uses a duplicate day whose earlier entry must not win.
    for clients, top in (
        ([stripe("split", 6, 1), stripe("single", 10, 1), stripe("split", 6, 1)], "split"),
        ([stripe("a", 10, 1), stripe("z", 10, 2)], "z"),
        ([stripe("z", 10, 2), stripe("a", 10, 2)], "a"),
        ([stripe("z", 0, 0), stripe("a", 0, 0)], "a"),
        ([stripe("overflow", 2**63 - 1, 0, output=1), stripe("normal", 100, 0)], "overflow"),
        ([stripe("overflow", 2**63 - 1, 0), stripe("overflow", 1, 0),
          stripe("normal", 100, 0)], "overflow"),
        ([stripe("negative", -2**63, 0, output=-1), stripe("normal", 0, 0)], "normal"),
        ([], None),
    ):
        value = copy.deepcopy(original)
        value["graph"]["contributions"][-2]["clients"] = [stripe("earlier", 2**63 - 1, 100)]
        value["graph"]["contributions"][-1]["clients"] = clients
        value["cases"] = [dict(name="top", fn="todayTopClient", now=now),
                          dict(name="tokens", fn="todayTokens", now=now),
                          dict(name="cost", fn="todayCost", now=now),
                          dict(name="absent", fn="todayTopClient", now="2026-07-11T12:00:00Z")]
        run(value, dict(top=top, tokens=4200, cost=1.23, absent=None))

    for invalid in ({}, dict(graph=[], cases=[]), dict(graph=original["graph"], cases=[True])):
        run(invalid, exit_code=1)
    for invalid in (True, "9223372036854775807", 1.5, 2**63):
        value = copy.deepcopy(original)
        value["graph"]["contributions"][-1]["totals"]["tokens"] = invalid
        run(value, exit_code=1)
