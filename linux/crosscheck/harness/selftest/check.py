"""Synthetic comparator wiring and fail-closed harness invocation checks."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from usage_pace import check_usage_pace
from format_selector import check_format


def run(command, expected=0, env=None):
    result = subprocess.run(command, capture_output=True, text=True, env=env, check=False)
    if result.returncode != expected:
        raise RuntimeError(
            f"{command}: expected exit {expected}, got {result.returncode}\n"
            f"{result.stdout}{result.stderr}"
        )
    return result


def main():
    harness, work = sys.argv[1:]
    here = Path(__file__).resolve().parent
    crosscheck = here.parents[1]
    Path(work).mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=work) as scratch:
        scratch = Path(scratch)
        runner = crosscheck / "harness/run_selector.py"

        def compare(fixtures, golden=here / "golden", expected=0):
            return run([sys.executable, str(runner), str(here / "emit.py"),
                        str(fixtures), str(golden), str(scratch / "synthetic"),
                        "synthetic"], expected=expected)

        result = compare(here / "actual")
        if "CROSSCHECK OK" not in result.stdout:
            raise RuntimeError("diff.py did not report a comparison")
        # A changed byte, adjacent Int64, boolean-as-number, or material float
        # drift must each reach the vendored comparator and fail.
        bad = scratch / "bad"
        bad.mkdir()
        for key, value in (("label", "Synthetic 台湾"), ("count", 9223372036854775806),
                           ("ready", 1), ("fraction", 0.301)):
            data = json.loads((here / "actual/synthetic.actual.json").read_text(encoding="utf-8"))
            data["synthetic"][key] = value
            (bad / "synthetic.actual.json").write_text(json.dumps(data), encoding="utf-8")
            compare(bad, expected=1)
        result = compare(here / "actual", golden=scratch / "missing", expected=77)
        if "golden missing" not in result.stdout or "not ported" in result.stdout:
            raise RuntimeError("ported selector without an oracle did not skip")
        (bad / "synthetic.actual.json").write_text('{"error":"not ported"}', encoding="utf-8")
        result = compare(bad, expected=77)
        if "not ported" not in result.stdout or "golden missing" in result.stdout:
            raise RuntimeError("unported selector with an oracle did not skip")

        fixtures = str(crosscheck / "fixtures")
        out = scratch / "out"
        base = [harness, fixtures, str(out)]
        for selector in ("usage-pace", "format", "provider-quota-pace-v3"):
            run([*base, selector])
            output = json.loads((out / f"{selector}.actual.json").read_text())
            if selector == "usage-pace":
                if len(output) != 42 or output.get("clamp-used-over-100") != {"rejected": True}:
                    raise RuntimeError("usage-pace did not decode the legacy fixture")
            elif selector == "format":
                if len(output) != 74 or output.get("exact-int64-max") != "9,223,372,036,854,775,807":
                    raise RuntimeError("format did not preserve exact token formatting")
            else:
                fixture = json.loads((crosscheck / "fixtures" / f"{selector}.json").read_text())
                if (output.get("schemaVersion") != 3
                        or set(output.get("cases", {})) != {c["name"] for c in fixture["cases"]}
                        or len(output.get("lifecycle", [])) != sum(len(a["windows"]) for a in fixture["payload"]["agents"])):
                    raise RuntimeError(f"incomplete provider output: {selector}")
        check_usage_pace(harness, scratch)
        check_format(harness, scratch)
        for selector in ([], ["all"]):
            legacy = scratch / ("default" if not selector else "all")
            run([harness, fixtures, str(legacy), *selector])
            if {p.name for p in legacy.iterdir()} != {"usage-pace.actual.json", "format.actual.json"}:
                raise RuntimeError("legacy selector set changed")

        for args, code in (([], 2), ([fixtures], 2), ([*base[1:], "unknown"], 2),
                           ([*base[1:], "format", "extra"], 2),
                           ([*base[1:], "--unknown"], 2),
                           ([*base[1:], "format", "-extra"], 2),
                           ([*base[1:], "format", "-AppleLanguages"], 2),
                           ([*base[1:], "format", "-AppleLanguages", "(zh-Hant)"], 1)):
            run([harness, *args], expected=code)
        run([harness, "-AppleLanguages", "(en)", fixtures, str(out), "format"])
        for zone in (None, "UTC", "Asia/Taipei "):
            env = dict(os.environ)
            env.pop("TZ", None)
            if zone is not None:
                env["TZ"] = zone
            run([*base, "format"], expected=1, env=env)
        blocked = scratch / "file"
        blocked.write_text("synthetic", encoding="utf-8")
        run([harness, fixtures, str(blocked), "format"], expected=1)

        for selector in ("format", "usage-pace", "provider-quota-pace-v3"):
            result = run([sys.executable, str(runner), harness, fixtures,
                          str(scratch / "missing"), str(scratch / selector), selector], expected=77)
            if "not ported" in result.stdout or "golden missing" not in result.stdout:
                raise RuntimeError(f"ported {selector} did not skip only for the absent oracle")
    print("crosscheck self-test OK (diff success/failure, CLI, timezone, skips)")


if __name__ == "__main__":
    main()
