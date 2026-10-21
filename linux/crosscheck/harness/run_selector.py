"""Run one selector, then compare with the read-only macOS oracle if ported."""

import json
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    harness, fixtures, golden, work, selector = sys.argv[1:]
    work = Path(work)
    actual_dir = work / "actual"
    expected_dir = work / "expected"
    actual_dir.mkdir(parents=True, exist_ok=True)
    expected_dir.mkdir(parents=True, exist_ok=True)
    # Only this selector's output may participate; never accept a stale result.
    actual = actual_dir / f"{selector}.actual.json"
    actual.unlink(missing_ok=True)
    result = subprocess.run([harness, fixtures, str(actual_dir), selector], check=False)
    if result.returncode:
        return 1
    output = json.loads(actual.read_text(encoding="utf-8"))
    oracle = Path(golden) / f"{selector}.json"
    reasons = []
    if not oracle.is_file():
        reasons.append("macOS oracle golden missing")
    if output == {"error": "not ported"}:
        reasons.append("not ported")
    if reasons:
        print(f"SKIP {selector}: {', '.join(reasons)}", flush=True)
        return 77

    # diff.py accepts directories containing matching *.actual.json names.
    # Stage a copy only; macOS is the sole writer of crosscheck/golden/.
    shutil.copyfile(oracle, expected_dir / actual.name)
    diff = Path(__file__).resolve().parents[1] / "diff.py"
    return subprocess.run(
        [sys.executable, str(diff), str(expected_dir), str(actual_dir)], check=False
    ).returncode


if __name__ == "__main__":
    sys.exit(main())
