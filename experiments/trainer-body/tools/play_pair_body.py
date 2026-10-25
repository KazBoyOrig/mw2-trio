"""PRIVATE opt-in genuine-body developer launcher: exact build184, trainer/pit, two players.

Default PLAN_ONLY. The sibling launcher is an immutable snapshot of the reviewed
public CLI. This wrapper enables only the extra guarded body-assets flag/log and
pins the tested private DLL plus the own V11 probe/manifest. It supplies no game
asset, Steam dependency, download, security change or native memory operation.
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import stat
import sys
import types


PRIVATE_REPO = Path(__file__).resolve().parents[1]
BASE_TOOL_SHA = "ca7696055dbb0ce6b4bf9f35819ca67b0c20f1e993dc9ce227c76509f0a4b77d"
DLL_SHA = "be51d30339edd669e67525ec0f0fada4f481d09ae56f8af509248fb181c37f7b"
MANIFEST_SHA = "91e8007280e7c6cfb4d0abd758fa6aaf477b800779f3555e1ed11c48a7e3fe6e"
PROBE_SHA = "7fab32a1a26b16d083036766949a249c0bfdb1dc24a62fe0795e1b0c4aee9722"
BODY_FLAG = "mw2-trio-body-assets-test.enable"
BODY_LOG = "bodyassets184.log"
PROFILE = "PRIVATE_EXPERIMENTAL_TRAINER_BODY_V11_BE51"


def load_sibling():
    """Execute only captured, pinned OWN Python bytes; no game/DLL is loaded."""
    path = PRIVATE_REPO / "tools/play_pair.py"
    info = path.lstat()
    if (not stat.S_ISREG(info.st_mode) or stat.S_ISLNK(info.st_mode)
            or getattr(info, "st_file_attributes", 0) & 0x400 or info.st_nlink != 1):
        raise RuntimeError("Private sibling launcher must be a single regular file")
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != BASE_TOOL_SHA:
        raise RuntimeError("Private sibling launcher snapshot differs; stop and review it")
    module = types.ModuleType("_mw2trio_private_body_base")
    module.__file__ = str(path)
    module.__package__ = ""
    # Compile the verified snapshot, avoiding a second source-file read at import.
    exec(compile(data, str(path), "exec"), module.__dict__)
    return module


launcher = load_sibling()
_BASE_BUNDLE = launcher.bundle
_BASE_PLAY = launcher.play


def body_bundle(repo):
    """No launcher may substitute another DLL, probe or manifest silently."""
    fixed = (("build/winmm.dll", DLL_SHA, "DLL"),
             ("content/framework/manifest.json", MANIFEST_SHA, "framework manifest"),
             ("content/framework/scripts/maps/_mw2trio_probe.gsc", PROBE_SHA, "own V11 probe"))
    for name, expected, label in fixed:
        if launcher.digest(repo / name) != expected:
            raise launcher.Refused("Private body profile requires its exact tested " + label)
    # Retains PE32, passing exact-DLL helper receipt, full framework inventory,
    # duplicate/path/hardlink and every individual file hash check from the CLI.
    result = _BASE_BUNDLE(repo)
    if result["dll_sha256"] != DLL_SHA:
        raise launcher.Refused("Private body DLL changed during bundle validation")
    for name, expected in (("manifest.json", MANIFEST_SHA),
                           ("scripts/maps/_mw2trio_probe.gsc", PROBE_SHA)):
        if result["framework"].get(name, {}).get("sha256") != expected:
            raise launcher.Refused("Private body framework changed during bundle validation")
    return result


def body_play(args, repo=PRIVATE_REPO, **kwargs):
    result = _BASE_PLAY(args, repo=repo, **kwargs)
    result["private_body_profile"] = PROFILE
    result["body_probe_sha256"] = PROBE_SHA
    result["body_manifest_sha256"] = MANIFEST_SHA
    result["body_status_claim"] = "Opt-in requested; inspect private proof/model logs, not an automatic rendering PASS"
    return result


# These are the sibling CLI's managed backup/stage/restore lists. The additional
# flag and log receive the same lifecycle; existing foreign framework is refused.
launcher.FLAGS = (*launcher.FLAGS, BODY_FLAG)
launcher.LOGS = (*launcher.LOGS, BODY_LOG)
launcher.bundle = body_bundle
launcher.play = body_play
launcher.__doc__ = __doc__


def main(argv=None):
    return launcher.main(argv)


if __name__ == "__main__":
    sys.exit(main())
