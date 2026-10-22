"""Local semantic checks; macOS-generated goldens remain the parity gate."""

import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    harness, work = sys.argv[1:]
    fixtures = Path(__file__).resolve().parents[1] / "fixtures"
    fixture = json.loads((fixtures / "quota-trend.json").read_text())
    with tempfile.TemporaryDirectory(dir=work) as scratch:
        scratch = Path(scratch)
        out = scratch / "out"

        def run(source=fixtures, expected=0):
            result = subprocess.run(
                [harness, str(source), str(out), "quota-trend"],
                env=dict(os.environ, TZ="Asia/Taipei"),
                capture_output=True, text=True, check=False,
            )
            assert result.returncode == expected, result.stdout + result.stderr

        run()
        actual = json.loads((out / "quota-trend.actual.json").read_text())
        assert set(actual) == {row["id"] for rows in fixture.values() for row in rows}

        def heat(name, slots, unplaced=0, days=1):
            value = actual["heatmap-" + name]
            assert len(value["cells"]) == 7 and all(len(row) == 24 for row in value["cells"])
            for day, row in enumerate(value["cells"]):
                for hour, cell in enumerate(row):
                    assert math.isclose(cell, slots.get((day, hour), 0), abs_tol=1e-12), (name, day, hour, cell)
            total = sum(slots.values())
            assert math.isclose(value["total"], total, abs_tol=1e-12)
            assert math.isclose(value["peak"], max(slots.values(), default=0), abs_tol=1e-12)
            assert value["unplacedPercent"] == unplaced and value["observedDays"] == days
            assert value["isEmpty"] == (total <= 0)
            assert value["hasMovement"] == (total > 0 or unplaced > 0)

        heat("empty", {}, days=0)
        heat("single", {})
        heat("weighted-hours", {(0, 9): 6, (0, 10): 12, (0, 11): 6})
        heat("local-midnight", {(6, 23): 6, (0, 0): 6}, days=2)
        heat("utc-midnight-one-local-day", {(0, 7): 4, (0, 8): 4})
        heat("six-hours", {(0, hour): 5 for hour in range(6)})
        heat("gap-only-fractional", {}, unplaced=0.5)
        heat("correction-and-duplicate", {(0, 11): 6})
        heat("cycle-boundaries", {(0, 10): 10, (0, 12): 4})
        heat("placed-and-gap", {(0, 0): 4}, unplaced=10)
        heat("dst-spring", {(6, 1): 6, (6, 3): 6})
        heat("dst-fall", {(6, 0): 4, (6, 1): 16, (6, 2): 4})
        heat("fractional-offset", {(0, 9): 5, (0, 10): 5})
        heat("negative-epoch", {(0, 9): 3, (0, 10): 3})

        nil_cases = ("empty", "single", "zero-duration", "reversed-window", "at-start",
                     "before-start", "duplicates-only", "outside-window", "lookback-excludes-old")
        for name in nil_cases:
            assert actual["trend-" + name] is None, name

        def trend(name, direction, projected, delta):
            value = actual["trend-" + name]
            assert value["direction"] == direction, name
            assert math.isclose(value["projectedUsedPercent"], projected, abs_tol=1e-10), name
            assert math.isclose(value["projectedDeltaPercent"], delta, abs_tol=1e-10), name
            assert value["runsOutEarly"] == (projected > 100), name

        trend("rising-unsorted", "rising", 90, 50)
        trend("current-reading-lags", "rising", 95, 50)
        trend("runs-out", "rising", 155, 75)
        trend("exact-100", "rising", 100, 50)
        trend("falling", "falling", 15, -25)
        trend("falling-floored", "falling", 0, -10)
        trend("flat", "flat", 40, 0)
        trend("flat-threshold", "flat", 13.75, 3.75)
        trend("above-flat-threshold", "rising", 13.7503, 3.7503)
        trend("negative-flat-threshold", "flat", 6.25, -3.75)
        trend("lookback-inclusive", "rising", 80, 40)
        trend("newest-anchors-lookback", "rising", 60, 20)
        trend("round-half-away", "rising", 80, 50)
        trend("duplicate-then-rise", "rising", 90, 50)
        trend("after-reset", "rising", 40, 0)
        trend("long-window-same-scale", "rising", 90, 50)

        # Fail closed on missing/malformed fixtures; no partial output replaces
        # an existing successful result when decoding fails.
        synthetic = scratch / "fixtures"
        synthetic.mkdir()
        run(synthetic, expected=1)
        path = synthetic / "quota-trend.json"
        for text in ("{", "[]", '{"heatmap": [], "trend": {}}'):
            path.write_text(text)
            run(synthetic, expected=1)
        for mutate in (
            lambda f: f["heatmap"][0].update(timeZone="Invalid/Synthetic"),
            lambda f: f["trend"][0].update(id=f["heatmap"][0]["id"]),
            lambda f: f["heatmap"][1]["points"][0].update(usedPercent=101),
            lambda f: f["heatmap"][1]["points"][0].pop("isActiveGroup"),
            lambda f: f["trend"][1]["samples"][0].update(atMs=0.5),
        ):
            copy = json.loads(json.dumps(fixture))
            mutate(copy)
            path.write_text(json.dumps(copy))
            run(synthetic, expected=1)
        assert json.loads((out / "quota-trend.actual.json").read_text()) == actual
    print(f"quota-trend semantic self-test OK ({len(actual)} synthetic cases, invalid inputs)")


if __name__ == "__main__":
    main()
