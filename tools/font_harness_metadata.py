# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

"""Portable capture metadata. Font result validation belongs to skity-font."""

import hashlib
import json
import locale
import os
from pathlib import Path
import platform
import struct


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False) + "\n",
                    encoding="utf-8")


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(data)
    return digest.hexdigest()


def json_fingerprint(value):
    """case-json-v1; shared wire contract with artifact/fingerprint.cc."""
    def encode(v):
        if v is None:
            return b"n"
        if isinstance(v, bool):
            return b"t" if v else b"f"
        if isinstance(v, (int, float)):
            return b"d" + struct.pack(">d", float(v) if v else 0.0)
        if isinstance(v, str):
            raw = v.encode("utf-8")
            return b"s" + str(len(raw)).encode() + b":" + raw
        if isinstance(v, list):
            return b"a" + str(len(v)).encode() + b":" + b"".join(encode(x) for x in v)
        if isinstance(v, dict):
            return b"o" + str(len(v)).encode() + b":" + b"".join(
                encode(k) + encode(v[k]) for k in sorted(v))
        raise TypeError("unsupported JSON value")
    return hashlib.sha256(encode(value)).hexdigest()


def font_fingerprints(case, repo):
    result = []
    repo = Path(repo).resolve()
    for font in case.get("font_files", []):
        uri = font["uri"]
        if not uri.startswith("repo://"):
            raise ValueError("font URI must start with repo://")
        path = (repo / uri[7:]).resolve()
        if not path.is_relative_to(repo):
            raise ValueError("font URI escapes repository")
        result.append({"uri": uri, "collection_index": font.get("collection_index", 0),
                       "sha256": sha256(path)})
    return result


def input_fingerprint(case, repo, environment=None):
    result = {"format": "case-json-v1", "case_sha256": json_fingerprint(case),
              "font_files": font_fingerprints(case, repo)}
    if not case.get("font_files"):
        if environment is None:
            raise ValueError("system font capture requires an environment snapshot")
        result["environment_sha256"] = json_fingerprint(environment)
    return result


def fontconfig_fingerprint(inventory, repo):
    """Capture the exact files and configuration loaded by the reporting engine."""
    if not isinstance(inventory, dict) or type(inventory.get("version")) is not int:
        raise ValueError("missing Fontconfig version")
    repo = Path(repo).resolve()
    snapshot = {"version": inventory["version"]}
    for key in ("files", "config_files"):
        names = inventory[key]
        if not isinstance(names, list) or not all(isinstance(name, str) for name in names):
            raise ValueError("invalid Fontconfig " + key)
        entries = []
        for path in sorted({Path(name).resolve() for name in names}):
            identity = "repo://" + path.relative_to(repo).as_posix() if path.is_relative_to(repo) else path.as_posix()
            if path.is_file():
                entries.append({"file": identity, "sha256": sha256(path)})
            elif key == "config_files" and path.is_dir():
                entries.append({"file": identity, "directory": sorted(p.name for p in path.iterdir())})
            else:
                raise ValueError("Fontconfig input is missing: " + str(path))
        snapshot[key] = entries
    return snapshot


def capture_environment(repo, backend, environ=None, fontconfig_inventory=None):
    """Collect host inputs once per invocation, without judging font results."""
    repo = Path(repo).resolve()
    environ = os.environ if environ is None else environ
    env = {"schema_version": 1, "platform": platform.system(),
           "os_version": platform.version(), "locale": list(locale.getlocale()),
           "locale_env": {key: environ.get(key, "") for key in
                          ("LANG", "LC_ALL", "LC_CTYPE")}, "fonts": []}
    files = set()
    roots = []
    if backend == "fontconfig":
        # The engine reports its runtime version and effective config/font lists.
        # An unavailable backend has no inventory and will fail at the native probe.
        if fontconfig_inventory is not None:
            env["fontconfig"] = fontconfig_fingerprint(fontconfig_inventory, repo)
            env["fonts"] = env["fontconfig"]["files"]
        return env
    elif backend == "directwrite":
        roots = [Path(environ.get("WINDIR", "C:/Windows")) / "Fonts",
                 Path(environ.get("LOCALAPPDATA", Path.home())) / "Microsoft/Windows/Fonts"]
        if platform.system() == "Windows":
            import winreg
            for hive in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
                try:
                    with winreg.OpenKey(hive, r"SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts") as key:
                        for i in range(winreg.QueryInfoKey(key)[1]):
                            name = winreg.EnumValue(key, i)[1]
                            if isinstance(name, str):
                                file = Path(os.path.expandvars(name))
                                files.add((file if file.is_absolute() else roots[0] / file).resolve())
                except FileNotFoundError:
                    pass
    elif backend == "coretext":
        roots = [Path("/System/Library/Fonts"), Path("/Library/Fonts"),
                 Path.home() / "Library/Fonts", Path("/System/Library/AssetsV2")]
    roots.extend(Path(x) for x in environ.get("SKITY_FONT_HARNESS_EXTRA_FONT_DIRS", "").split(os.pathsep) if x)
    for root in roots:
        if root.is_dir():
            for base, _, names in os.walk(root):
                for name in names:
                    path = Path(base) / name
                    if path.suffix.lower() in (".ttf", ".ttc", ".otf", ".otc", ".dfont", ".pfb", ".conf"):
                        files.add(path.resolve())
    for path in sorted(files):
        if not path.is_file():
            raise FileNotFoundError("registered font/config unavailable: " + str(path))
        if path.is_relative_to(repo):
            identity = "repo://" + path.relative_to(repo).as_posix()
        elif path.is_relative_to(Path.home()):
            identity = "user://" + path.relative_to(Path.home()).as_posix()
        else:
            identity = path.as_posix()
        env["fonts"].append({"file": identity, "sha256": sha256(path)})
    return env


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Capture current font environment for skity-font --environment")
    parser.add_argument("--repo-root", default=str(Path(__file__).resolve().parents[1]))
    parser.add_argument("--backend", required=True, choices=["freetype", "fontconfig", "directwrite", "coretext"])
    parser.add_argument("--out", required=True)
    parser.add_argument("--fontconfig-env", help="Current engine env-info JSON for the fontconfig backend")
    options = parser.parse_args()
    if options.backend == "fontconfig" and not options.fontconfig_env:
        parser.error("--fontconfig-env is required for the fontconfig backend")
    inventory = read_json(options.fontconfig_env)["fontconfig_inventory"] if options.fontconfig_env else None
    write_json(options.out, capture_environment(options.repo_root, options.backend, fontconfig_inventory=inventory))
