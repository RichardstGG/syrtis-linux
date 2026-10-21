"""Check the quota-trend extension and fail-closed CLI on macOS."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    harness, work = sys.argv[1:]
    env = dict(os.environ, TZ="Asia/Taipei")
    with tempfile.TemporaryDirectory(dir=work) as scratch:
        fixtures = Path(scratch) / "fixtures"
        fixtures.mkdir()
        source = Path(__file__).resolve().parent.parent / "fixtures/quota-trend.json"
        shutil.copyfile(source, fixtures / source.name)
        out = Path(scratch) / "out"
        base = [str(fixtures), str(out)]
        pin = ["-AppleLanguages", "(en)"]

        def run(args, expected=0, zone="Asia/Taipei"):
            result = subprocess.run(
                [harness, *args], env=dict(env, TZ=zone),
                capture_output=True, text=True, check=False,
            )
            if result.returncode != expected:
                raise RuntimeError(
                    f"{args}: expected exit {expected}, got {result.returncode}\n"
                    f"{result.stdout}{result.stderr}"
                )

        # The extension's all/default run owns only its new selector.
        run([*base, *pin])
        actual = json.loads((out / "quota-trend.actual.json").read_text())
        fixture = json.loads(source.read_text())
        if set(actual) != {row["id"] for rows in fixture.values() for row in rows}:
            raise RuntimeError("quota-trend case set differs from fixture")
        if actual["trend-runs-out"]["runsOutEarly"] is not True or actual["trend-empty"] is not None:
            raise RuntimeError("quota-trend did not call the shipping trend fold")
        if actual["heatmap-gap-only-fractional"]["hasMovement"] is not True:
            raise RuntimeError("quota-trend did not call the shipping heatmap fold")
        run([*pin, *base, "all"])
        run([*base, "quota-trend", *pin])
        if {p.name for p in out.iterdir()} != {"quota-trend.actual.json"}:
            raise RuntimeError("extension produced unexpected selectors")
        if json.loads((out / "quota-trend.actual.json").read_text()) != actual:
            raise RuntimeError("explicit selector differs from all/default")
        for selector in ("unknown", "usage-pace", "format", "provider-quota-pace-v3"):
            run([*base, selector, *pin], expected=2)
        for args in (pin, [*base, "all", "extra", *pin],
                     [*base, "--unknown", *pin], [*base, "-AppleLanguages"]):
            run(args, expected=2)
        run(base, expected=1)
        run([*base, "-AppleLanguages", "(zh-Hant)"], expected=1)
        run([*base, *pin], expected=1, zone="UTC")
        blocked = Path(scratch) / "file"
        blocked.write_text("synthetic", encoding="utf-8")
        run([str(fixtures), str(blocked), *pin], expected=1)
        (fixtures / source.name).write_text("{", encoding="utf-8")
        run([*base, "quota-trend", *pin], expected=1)
    print("Linux Swift harness self-test OK (quota-trend, CLI, timezone, I/O)")


if __name__ == "__main__":
    main()
