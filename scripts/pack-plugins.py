#!/usr/bin/env python3
"""Build, smoke-test, and publish Wilfred gallery plugins.

- Validates every plugins/<id>/ (manifest, README, sources).
- Compiles native plugins for the CURRENT OS, smoke-tests the query
  protocol via ctypes, and writes a byte-reproducible zip to
  plugins/dist/<id>-<version>-<os>.zip (fixed timestamps, sorted entries).
- Updates plugins/registry.json artifacts for this OS (sha256 of the zip).

Usage (from the repo root):
    python3 scripts/pack-plugins.py [--validate-only] [--no-write] [--id dice]
    python3 scripts/pack-plugins.py --assemble <dir-with-per-os-zips>

CI (.github/workflows/plugins.yml) runs this on Windows/macOS/Linux and
commits the result, so contributors only ever submit source.
Official download base: https://ajakfial.github.io/Wilfred/plugins/
"""
import ctypes
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PLUGINS = ROOT / "plugins"
DIST = PLUGINS / "dist"
REGISTRY = PLUGINS / "registry.json"
BASE_URL = "https://ajakfial.github.io/Wilfred/plugins/"

ID_RE = re.compile(r"^[a-z0-9][a-z0-9-]*$")
VER_RE = re.compile(r"^\d+\.\d+\.\d+$")

# id -> queries that must return at least one result.
SMOKE = {
    "dice": ["3d6", "d20+2", "dice"],
    "morse": ["morse hello", "morse .... ..", "morse"],
    "password": ["password", "password 24", "password words 3", "password pin"],
    "cheatsheets": ["cheat", "cheat git", "cheat git rebase"],
}

_os = platform.system().lower()
OS_TAG = "windows" if _os == "windows" else ("macos" if _os == "darwin" else "linux")


def fail(msg):
    print(f"pack-plugins: error: {msg}", file=sys.stderr)
    return False


def read_manifest(d):
    p = d / "plugin.yml"
    if not p.exists():
        return None, f"{d.name}: missing plugin.yml"
    data = {}
    for line in p.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if ":" not in line:
            return None, f"{d.name}: bad manifest line: {line!r}"
        k, v = line.split(":", 1)
        data[k.strip()] = v.strip().strip('"').strip("'")
    return data, ""


def validate(d):
    pid = d.name
    if not ID_RE.match(pid):
        return fail(f"{pid}: id must match [a-z0-9][a-z0-9-]*")
    m, err = read_manifest(d)
    if m is None:
        return fail(err)
    if m.get("id", pid) != pid:
        return fail(f"{pid}: manifest id must equal the directory name")
    if m.get("kind", "native") not in ("native", "stdio"):
        return fail(f"{pid}: kind must be native or stdio")
    if not VER_RE.match(m.get("version", "")):
        return fail(f"{pid}: version must be semver (x.y.z)")
    if not m.get("description"):
        return fail(f"{pid}: description is required")
    if not (d / "README.md").exists():
        return fail(f"{pid}: README.md is required")
    if m.get("kind", "native") == "native":
        srcs = sorted(d.glob("*.c")) + sorted(d.glob("*.cpp"))
        if not srcs:
            return fail(f"{pid}: native plugin needs .c/.cpp sources")
    else:
        if not m.get("command"):
            return fail(f"{pid}: stdio plugin needs command:")
    return True


def find_cc():
    for c in (os.environ.get("CC"), "cc", "gcc", "clang", "cl"):
        if c and shutil.which(c):
            return c
    return None


def compile_native(d, m, work):
    srcs = sorted(str(p) for p in list(d.glob("*.c")) + list(d.glob("*.cpp")))
    pid = d.name
    if OS_TAG == "windows":
        lib = f"{pid}.dll"
        out = str(work / lib)
    elif OS_TAG == "macos":
        lib = f"{pid}.dylib"
        out = str(work / lib)
    else:
        lib = f"{pid}.so"
        out = str(work / lib)
    cc = find_cc()
    if not cc:
        return None, "no C compiler found (cc/gcc/clang/cl)"
    base = os.path.basename(cc).lower()
    if "cl" in base and "clang" not in base:
        cmd = [cc, "/nologo", "/W4", "/LD", f"/Fe{out}"] + srcs
    else:
        shared = ["-dynamiclib"] if OS_TAG == "macos" else ["-shared"]
        cmd = [cc, "-O2", "-Wall", "-Wextra", "-fPIC"] + shared + ["-o", out] + srcs
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=str(work))
    if r.returncode != 0 or not Path(out).exists():
        return None, f"compile failed: {(r.stderr or r.stdout)[-2000:]}"
    return out, ""


def check_response(raw, queries):
    try:
        doc = json.loads(raw)
    except Exception as e:
        return False, f"invalid JSON: {e}"
    if not isinstance(doc, dict) or not isinstance(doc.get("results"), list):
        return False, "response needs {results: [...]}"
    for r in doc["results"]:
        if not isinstance(r, dict) or not (r.get("title") or r.get("path")):
            return False, "result needs title or path"
    return True, ""


def smoke_test(lib, pid):
    try:
        dll = ctypes.CDLL(lib)
    except Exception as e:
        return fail(f"{pid}: cannot load {lib}: {e}")
    try:
        dll.wilfred_plugin_abi.restype = ctypes.c_int
        if dll.wilfred_plugin_abi() != 1:
            return fail(f"{pid}: bad ABI version")
        q = dll.wilfred_plugin_query
        q.argtypes = [ctypes.c_char_p]
        q.restype = ctypes.c_char_p
    except Exception as e:
        return fail(f"{pid}: missing ABI exports: {e}")
    for query in SMOKE.get(pid, ["test"]):
        req = json.dumps({"op": "query", "q": query, "limit": 5}).encode()
        try:
            raw = q(req)
        except Exception as e:
            return fail(f"{pid}: query {query!r} crashed: {e}")
        if not raw:
            return fail(f"{pid}: query {query!r} returned NULL")
        ok, err = check_response(raw.decode("utf-8", "replace"), query)
        if not ok:
            return fail(f"{pid}: query {query!r}: {err}")
        if pid in SMOKE:
            # Known flagships must answer their trigger queries (usage cards
            # count — every SMOKE entry above returns at least one result).
            doc = json.loads(raw.decode("utf-8", "replace"))
            if not doc["results"]:
                return fail(f"{pid}: query {query!r} returned no results")
    return True


def write_zip(lib_path, manifest_text, dest):
    with zipfile.ZipFile(dest, "w", zipfile.ZIP_DEFLATED) as z:
        for name, data in sorted(
            [(os.path.basename(lib_path), Path(lib_path).read_bytes()),
             ("plugin.yml", manifest_text.encode("utf-8"))],
            key=lambda t: t[0],
        ):
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, data)


def sha256_file(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def pack_one(d, write_registry=True):
    pid = d.name
    m, err = read_manifest(d)
    if m is None:
        return fail(err)
    kind = m.get("kind", "native")
    version = m["version"]
    if kind != "native":
        print(f"{pid}: stdio plugins ship as source (nothing to compile)")
        return True
    with tempfile.TemporaryDirectory(prefix="wilf-pack-") as work:
        lib, err = compile_native(d, m, Path(work))
        if lib is None:
            return fail(f"{pid}: {err}")
        if not smoke_test(lib, pid):
            return False
        libname = os.path.basename(lib)
        manifest = (
            f"id: {pid}\nkind: native\nversion: {version}\n"
            f"description: {m['description']}\npath: ./{libname}\n"
        )
        DIST.mkdir(parents=True, exist_ok=True)
        dest = DIST / f"{pid}-{version}-{OS_TAG}.zip"
        write_zip(lib, manifest, str(dest))
        digest = sha256_file(str(dest))
    if not write_registry:
        print(f"{pid}: built+smoked {dest.name} (registry untouched)")
        return True
    reg = json.loads(REGISTRY.read_text(encoding="utf-8"))
    for e in reg.get("plugins", []):
        if e.get("id") == pid:
            e.setdefault("version", version)
            e.setdefault("kind", "native")
            e.setdefault("description", m["description"])
            arts = e.setdefault("artifacts", {})
            arts[OS_TAG] = {"url": BASE_URL + dest.name, "sha256": digest}
            break
    else:
        return fail(f"{pid}: not listed in plugins/registry.json")
    REGISTRY.write_text(json.dumps(reg, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"{pid}: packed {dest.name} sha256:{digest[:12]}...")
    return True


def assemble(staged):
    """Merge prebuilt per-OS zips (from CI matrix) into dist/ + registry."""
    staged = Path(staged)
    zips = sorted(staged.glob("*.zip"))
    if not zips:
        return fail(f"nothing staged in {staged}")
    reg = json.loads(REGISTRY.read_text(encoding="utf-8"))
    by_id = {e.get("id"): e for e in reg.get("plugins", [])}
    for z in zips:
        m = re.match(r"^([a-z0-9][a-z0-9-]*)-(\d+\.\d+\.\d+)-(windows|macos|linux)\.zip$", z.name)
        if not m:
            return fail(f"bad artifact name: {z.name}")
        pid, version, os_tag = m.groups()
        if pid not in by_id:
            return fail(f"{z.name}: unknown plugin {pid}")
        if by_id[pid].get("version") != version:
            return fail(f"{z.name}: version {version} != registry {by_id[pid].get('version')}")
        with zipfile.ZipFile(z) as zh:
            if zh.testzip() is not None:
                return fail(f"{z.name}: corrupt zip")
        DIST.mkdir(parents=True, exist_ok=True)
        shutil.copy2(z, DIST / z.name)
        arts = by_id[pid].setdefault("artifacts", {})
        arts[os_tag] = {"url": BASE_URL + z.name, "sha256": sha256_file(str(DIST / z.name))}
        print(f"{pid}/{os_tag}: staged {z.name}")
    REGISTRY.write_text(json.dumps(reg, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return True


def main():
    only = None
    validate_only = False
    no_write = False
    assemble_dir = None
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        a = args[i]
        if a == "--validate-only":
            validate_only = True
        elif a == "--no-write":
            no_write = True
        elif a.startswith("--id="):
            only = a[4:]
        elif a == "--assemble" and i + 1 < len(args):
            i += 1
            assemble_dir = args[i]
        else:
            print("usage: pack-plugins.py [--validate-only] [--no-write] [--id=<id>] [--assemble <dir>]")
            return 2
        i += 1
    if assemble_dir:
        return 0 if assemble(assemble_dir) else 1
    dirs = sorted(p for p in PLUGINS.iterdir() if p.is_dir() and (p / "plugin.yml").exists())
    if only:
        dirs = [p for p in dirs if p.name == only]
        if not dirs:
            return 1 if fail(f"no plugin {only}") else 2
    ok = True
    for d in dirs:
        if not validate(d):
            ok = False
    if not ok or validate_only:
        print("validate-only: " + ("OK" if ok else "FAIL"))
        return 0 if ok else 1
    for d in dirs:
        if not pack_one(d, write_registry=not no_write):
            ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
