"""Check the empty selector table and fail-closed CLI on macOS."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    harness, work = sys.argv[1:]
    env = dict(os.environ, TZ="Asia/Taipei")
    with tempfile.TemporaryDirectory(dir=work) as scratch:
        fixtures = Path(scratch) / "fixtures"
        fixtures.mkdir()
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

        # An empty table must never invent golden files or run root selectors.
        run([*base, *pin])
        run([*pin, *base, "all"])
        if list(out.iterdir()):
            raise RuntimeError("empty selector table produced output")
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
    print("Linux Swift harness self-test OK (empty selectors, CLI, timezone, I/O)")


if __name__ == "__main__":
    main()
