"""Check core-leaf, reports, and the fail-closed CLI on macOS."""

import json
import os
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "harness"))
from core_leaf_check import verify_output
from reports_check import verify_output as verify_reports_output
from charts_check import verify_output as verify_charts_output


def main():
    harness, work = sys.argv[1:]
    env = dict(os.environ, TZ="Asia/Taipei")
    with tempfile.TemporaryDirectory(dir=work) as scratch:
        fixtures = Path(scratch) / "fixtures"
        fixtures.mkdir()
        shutil.copyfile(Path(__file__).resolve().parents[1] / "fixtures/core-leaf.json", fixtures / "core-leaf.json")
        shutil.copyfile(Path(__file__).resolve().parents[1] / "fixtures/reports.json", fixtures / "reports.json")
        shutil.copyfile(Path(__file__).resolve().parents[1] / "fixtures/charts.json", fixtures / "charts.json")
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

        # The extension must never run root selectors or invent oracle goldens.
        run([*base, *pin])
        run([*pin, *base, "all"])
        if {p.name for p in out.iterdir()} != {"core-leaf.actual.json", "reports.actual.json", "charts.actual.json"}:
            raise RuntimeError("extension selector set changed")
        verify_output(out / "core-leaf.actual.json")
        verify_reports_output(out / "reports.actual.json")
        verify_charts_output(out / "charts.actual.json")
        charts_baseline = json.loads((out / "charts.actual.json").read_text())
        run([*base, "charts", *pin])
        if json.loads((out / "charts.actual.json").read_text()) != charts_baseline:
            raise RuntimeError("charts output depends on selector invocation")
        reports_baseline = json.loads((out / "reports.actual.json").read_text())
        run([*base, "reports", *pin])
        if json.loads((out / "reports.actual.json").read_text()) != reports_baseline:
            raise RuntimeError("reports output depends on selector invocation")
        baseline = json.loads((out / "core-leaf.actual.json").read_text())
        run([*base, "core-leaf", *pin])
        if json.loads((out / "core-leaf.actual.json").read_text()) != baseline:
            raise RuntimeError("core-leaf output depends on selector invocation")
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
    print("Linux Swift harness self-test OK (core-leaf, reports, CLI, timezone, I/O)")


if __name__ == "__main__":
    main()
