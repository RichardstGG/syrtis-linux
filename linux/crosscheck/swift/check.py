"""Check core-leaf, reports, agent-usage, and the fail-closed CLI on macOS."""

import json
import os
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "harness"))
from reports_check import verify_structure as verify_reports_structure
from agent_usage_check import verify_structure as verify_agent_structure
from attribution_check import verify_structure as verify_attribution_structure


def main():
    harness, work = sys.argv[1:]
    env = dict(os.environ, TZ="Asia/Taipei")
    with tempfile.TemporaryDirectory(dir=work) as scratch:
        fixtures = Path(scratch) / "fixtures"
        fixtures.mkdir()
        shutil.copyfile(Path(__file__).resolve().parents[1] / "fixtures/core-leaf.json", fixtures / "core-leaf.json")
        shutil.copyfile(Path(__file__).resolve().parents[1] / "fixtures/reports.json", fixtures / "reports.json")
        shutil.copyfile(Path(__file__).resolve().parents[1] / "fixtures/agent-usage.json", fixtures / "agent-usage.json")
        shutil.copyfile(Path(__file__).resolve().parents[1] / "fixtures/attribution.json", fixtures / "attribution.json")
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
        if {p.name for p in out.iterdir()} != {"core-leaf.actual.json", "reports.actual.json", "agent-usage.actual.json", "attribution.actual.json"}:
            raise RuntimeError("extension selector set changed")
        # Oracle checks validate transport/coverage, never guessed decoder or
        # calculation results. diff.py compares C++ semantics to these outputs.
        if not isinstance(json.loads((out / "core-leaf.actual.json").read_text()), dict):
            raise RuntimeError("invalid core-leaf output object")
        verify_reports_structure(out / "reports.actual.json", fixtures / "reports.json")
        verify_agent_structure(out / "agent-usage.actual.json", fixtures / "agent-usage.json")
        verify_attribution_structure(out / "attribution.actual.json", fixtures / "attribution.json")
        run([*base, "attribution", *pin])
        verify_attribution_structure(out / "attribution.actual.json", fixtures / "attribution.json")
        run([*base, "agent-usage", *pin])
        verify_agent_structure(out / "agent-usage.actual.json", fixtures / "agent-usage.json")
        run([*base, "reports", *pin])
        verify_reports_structure(out / "reports.actual.json", fixtures / "reports.json")
        run([*base, "core-leaf", *pin])
        if not isinstance(json.loads((out / "core-leaf.actual.json").read_text()), dict):
            raise RuntimeError("invalid core-leaf output object")
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
    print("Linux Swift harness self-test OK (core-leaf, reports, agent-usage, CLI, timezone, I/O)")


if __name__ == "__main__":
    main()
