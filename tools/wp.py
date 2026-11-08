#!/usr/bin/env python3
"""Waypoint Engine helper CLI.

One command for every job. Run `python tools/wp.py --help` for the list.

    python tools/wp.py doctor            # is my machine ready?
    python tools/wp.py new MyGame        # create a project
    python tools/wp.py build             # build the engine + runtime
    python tools/wp.py run MyGame        # play my project
    python tools/wp.py test              # run the unit tests
    python tools/wp.py export MyGame     # package it for someone else
    python tools/wp.py deps status       # are the pinned dependencies here?
    python tools/wp.py deps fetch        # download them

Only the Python standard library is used, so it works on a fresh machine.
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
LOCKFILE = REPO_ROOT / "external" / "dependencies.lock.json"
EXTERNAL_DIR = REPO_ROOT / "external"
PATCH_DIR = REPO_ROOT / "cmake" / "patches"
BUILD_ROOT = REPO_ROOT / "build"
DIST_ROOT = REPO_ROOT / "dist"
TEMPLATES_DIR = REPO_ROOT / "templates"

MIN_CMAKE = (3, 24)
MIN_PYTHON = (3, 8)
PROJECT_FILE = "waypoint.project.json"
NAME_TOKEN = "__PROJECT_NAME__"

IS_WINDOWS = os.name == "nt"
EXE_SUFFIX = ".exe" if IS_WINDOWS else ""


# ---------------------------------------------------------------------------
# Small terminal helpers
# ---------------------------------------------------------------------------
def supports_color() -> bool:
    return sys.stdout.isatty() and os.environ.get("NO_COLOR") is None


class Style:
    def __init__(self) -> None:
        on = supports_color()
        self.ok = "\033[92m" if on else ""
        self.warn = "\033[93m" if on else ""
        self.err = "\033[91m" if on else ""
        self.dim = "\033[2m" if on else ""
        self.bold = "\033[1m" if on else ""
        self.end = "\033[0m" if on else ""


STYLE = Style()


def say(message: str = "") -> None:
    print(message)


def step(message: str) -> None:
    print(f"{STYLE.bold}==>{STYLE.end} {message}")


def ok(message: str) -> None:
    print(f"  {STYLE.ok}OK{STYLE.end}    {message}")


def warn(message: str) -> None:
    print(f"  {STYLE.warn}WARN{STYLE.end}  {message}")


def fail(message: str) -> None:
    print(f"  {STYLE.err}FAIL{STYLE.end}  {message}")


def die(message: str) -> "NoReturn":  # noqa: F821 - forward reference is fine at runtime
    print(f"\n{STYLE.err}error:{STYLE.end} {message}", file=sys.stderr)
    raise SystemExit(1)


# ---------------------------------------------------------------------------
# Tool discovery
#
# We look on PATH first, then `~/waypoint-tools/bin`, then `<repo>/.waypoint-tools/bin`.
# That lets a user drop cmake or ninja next to the repo without touching PATH.
# ---------------------------------------------------------------------------
def tool_search_paths() -> list[Path]:
    paths: list[Path] = []
    env_path = os.environ.get("PATH", "")
    for entry in env_path.split(os.pathsep):
        if entry:
            paths.append(Path(entry))
    paths.append(Path.home() / "waypoint-tools" / "bin")
    paths.append(REPO_ROOT / ".waypoint-tools" / "bin")
    return paths


def find_tool(name: str) -> str | None:
    candidates = [name + EXE_SUFFIX]
    if not IS_WINDOWS:
        candidates.append(name)
    for directory in tool_search_paths():
        for candidate in candidates:
            path = directory / candidate
            if path.is_file() and os.access(path, os.X_OK):
                return str(path)
    return None


def tool_version(command: list[str]) -> str | None:
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=30, check=False)
    except (OSError, subprocess.SubprocessError):
        return None
    for output in (result.stdout, result.stderr):
        if re.search(r"\d+\.\d+", output):
            return output.strip().splitlines()[0]
    return None


def require_tools(*names: str) -> dict[str, str]:
    found: dict[str, str] = {}
    missing: list[str] = []
    for name in names:
        path = find_tool(name)
        if path:
            found[name] = path
        else:
            missing.append(name)
    if missing:
        die(
            "missing required tool(s): "
            + ", ".join(missing)
            + "\n  Install CMake >= 3.24 and Ninja, or put them in ~/waypoint-tools/bin.\n"
            "  Linux:  sudo apt install cmake ninja-build\n"
            "  macOS:  brew install cmake ninja\n"
            "  Windows: winget install Kitware.CMake && winget install Ninja-build.Ninja"
        )
    return found


def run(command: list[str], cwd: Path | None = None, env: dict[str, str] | None = None) -> int:
    printable = " ".join(str(part) for part in command)
    print(f"{STYLE.dim}$ {printable}{STYLE.end}")
    merged = os.environ.copy()
    if env:
        merged.update(env)
    try:
        return subprocess.run(command, cwd=str(cwd) if cwd else None, env=merged, check=False).returncode
    except FileNotFoundError:
        die(f"could not run '{command[0]}'")


# ---------------------------------------------------------------------------
# Project helpers
# ---------------------------------------------------------------------------
def project_manifest(project_dir: Path) -> dict:
    manifest = project_dir / PROJECT_FILE
    if not manifest.is_file():
        die(f"{project_dir} does not look like a Waypoint project (no {PROJECT_FILE})")
    try:
        data = json.loads(manifest.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        die(f"{manifest} is not valid JSON: {error}")
    if not isinstance(data, dict):
        die(f"{manifest} must contain a JSON object")
    return data


def resolve_project(argument: str) -> Path:
    path = Path(argument).expanduser()
    if not path.is_absolute():
        # Accept both "MyGame" and "MyGame/waypoint.project.json".
        for base in (Path.cwd(), REPO_ROOT, REPO_ROOT / "samples"):
            candidate = (base / path)
            if (candidate / PROJECT_FILE).is_file():
                return candidate.resolve()
        if path.name == PROJECT_FILE and path.is_file():
            return path.resolve().parent
        return (Path.cwd() / path).resolve()
    return path.resolve()


def headless_requested() -> bool:
    return os.environ.get("WAYPOINT_HEADLESS_BUILD", "").strip().lower() in {"1", "true", "yes", "on"}


def preset_name(release: bool = False, editor: bool = False, headless: bool | None = None) -> str:
    if headless is None:
        headless = headless_requested()
    if headless:
        return "headless"
    if editor:
        return "editor"
    if release:
        return "release"
    return "default"


def build_dir_for(preset: str) -> Path:
    return BUILD_ROOT / preset


def configure(preset: str, tools: dict[str, str]) -> None:
    cmake = tools["cmake"]
    if not (build_dir_for(preset) / "CMakeCache.txt").is_file():
        code = run([cmake, "--preset", preset], cwd=REPO_ROOT)
        if code != 0:
            die(f"configure failed (preset '{preset}')")


def build(preset: str, target: str | None = None) -> None:
    tools = require_tools("cmake", "ninja")
    configure(preset, tools)
    jobs = str(os.cpu_count() or 2)
    command = [tools["cmake"], "--build", "--preset", preset, "--parallel", jobs]
    if target:
        command += ["--target", target]
    if run(command, cwd=REPO_ROOT) != 0:
        die(f"build failed (preset '{preset}')")
    ok(f"build finished: {build_dir_for(preset)}")


def find_runtime_binary(preset: str) -> Path | None:
    candidates = [
        build_dir_for(preset) / "bin" / f"waypoint{EXE_SUFFIX}",
        build_dir_for(preset) / f"waypoint{EXE_SUFFIX}",
        build_dir_for(preset) / "runtime" / f"waypoint{EXE_SUFFIX}",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return None


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------
def command_doctor(args: argparse.Namespace) -> int:
    step("Waypoint Engine doctor")
    problems = 0

    python_ok = sys.version_info >= MIN_PYTHON
    (ok if python_ok else fail)(
        f"python {platform.python_version()} (need >= {'.'.join(map(str, MIN_PYTHON))})"
    )
    problems += 0 if python_ok else 1

    system = f"{platform.system()} {platform.release()} ({platform.machine()})"
    ok(f"platform {system}")

    for name in ("cmake", "ninja", "git"):
        path = find_tool(name)
        if not path:
            (warn if name == "git" else fail)(f"{name} not found")
            problems += 0 if name == "git" else 1
            continue
        version = tool_version([path, "--version"]) or "unknown version"
        if name == "cmake":
            match = re.search(r"(\d+)\.(\d+)", version)
            if match and (int(match.group(1)), int(match.group(2))) < MIN_CMAKE:
                fail(f"cmake {version} is too old (need >= 3.24)")
                problems += 1
            else:
                ok(f"cmake {version}")
        else:
            ok(f"{name} {version}")

    compiler = find_tool("clang++") or find_tool("g++") or find_tool("cl")
    if compiler:
        ok(f"compiler {compiler}")
    else:
        fail("no C++ compiler found (need clang++, g++, or cl)")
        problems += 1

    # Which pieces of the engine can actually build here?
    if headless_requested():
        ok("WAYPOINT_HEADLESS_BUILD is set: render/runtime/editor will be skipped")
    else:
        graphics = graphics_readiness()
        if graphics:
            warn("missing system packages for a windowed build: " + ", ".join(graphics))
            say(
                f"    {STYLE.dim}Debian/Ubuntu: sudo apt install libx11-dev libxext-dev "
                f"libxrandr-dev libxinerama-dev libxcursor-dev libxfixes-dev libxi-dev "
                f"libxss-dev libwayland-dev libxkbcommon-dev libegl1-mesa-dev libgl1-mesa-dev "
                f"libvulkan-dev{STYLE.end}"
            )
            say(
                f"    {STYLE.dim}No GPU here? Use: WAYPOINT_HEADLESS_BUILD=1 "
                f"python tools/wp.py test{STYLE.end}"
            )
        else:
            ok("window system and GL/Vulkan headers look present")
        if not qt6_available():
            say(
                f"  {STYLE.dim}note{STYLE.end}  Qt 6 not found - the editor is optional "
                "(`wp.py build --editor` needs it)"
            )

    deps = dependency_report()
    missing = [name for name, info in deps.items() if not info["present"]]
    if missing:
        warn(f"{len(missing)}/{len(deps)} dependencies not fetched yet")
        say(f"    {STYLE.dim}run: python tools/wp.py deps fetch{STYLE.end}")
    else:
        ok(f"all {len(deps)} dependencies present in external/")

    say("")
    if problems:
        say(f"{STYLE.err}doctor found {problems} blocking problem(s){STYLE.end}")
        return 1
    say(f"{STYLE.ok}Everything needed is installed.{STYLE.end}")
    return 0


def graphics_readiness() -> list[str]:
    """Best-effort check for the Linux desktop prerequisites."""
    if platform.system() != "Linux":
        return []
    include = Path("/usr/include")
    checks = {
        "libx11-dev": include / "X11" / "Xlib.h",
        "libgl1-mesa-dev": include / "GL" / "gl.h",
        "libvulkan-dev": include / "vulkan" / "vulkan.h",
    }
    return [name for name, path in checks.items() if not path.exists()]


def qt6_available() -> bool:
    for candidate in (
        REPO_ROOT / "build" / "editor" / "CMakeCache.txt",
        Path("/usr/lib/qt6"),
        Path("/usr/lib/x86_64-linux-gnu/cmake/Qt6"),
        Path("/opt/homebrew/opt/qt"),
    ):
        if candidate.exists():
            return True
    return False


def load_lockfile() -> dict:
    if not LOCKFILE.is_file():
        die(f"missing dependency lockfile: {LOCKFILE}")
    try:
        data = json.loads(LOCKFILE.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        die(f"{LOCKFILE} is not valid JSON: {error}")
    return data.get("dependencies", {})


def git_head(path: Path) -> str | None:
    if not (path / ".git").exists():
        return None
    try:
        result = subprocess.run(
            ["git", "-C", str(path), "rev-parse", "HEAD"],
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    return result.stdout.strip() if result.returncode == 0 else None


def dependency_patches(name: str) -> list[Path]:
    """Local patches applied on top of a pinned dependency checkout."""
    if not PATCH_DIR.is_dir():
        return []
    return sorted(PATCH_DIR.glob(f"{name}-*.patch"))


def patch_is_applied(directory: Path, patch: Path) -> bool:
    result = subprocess.run(
        ["git", "-C", str(directory), "apply", "--check", "--reverse", str(patch)],
        capture_output=True,
        text=True,
        timeout=60,
        check=False,
    )
    return result.returncode == 0


def apply_dependency_patches(name: str, directory: Path) -> None:
    """Applies cmake/patches/<name>-*.patch to a checkout, idempotently.

    A pin is meant to be pristine upstream code, so we patch in exactly one
    documented place and only for a defect that stops the pin from compiling.
    CMake applies the same files, so both dependency paths stay identical.
    """
    for patch in dependency_patches(name):
        if patch_is_applied(directory, patch):
            continue
        if run(["git", "-C", str(directory), "apply", str(patch)]) != 0:
            die(f"{name}: could not apply {patch.name}")
        say(f"  {STYLE.dim}{name}: applied {patch.name}{STYLE.end}")


def dependency_report() -> dict[str, dict]:
    report: dict[str, dict] = {}
    for name, pin in load_lockfile().items():
        directory = EXTERNAL_DIR / name
        present = (directory / ".git").exists()
        head = git_head(directory) if present else None
        patches = dependency_patches(name) if present else []
        report[name] = {
            "present": present and head is not None,
            "commit": pin.get("commit", ""),
            "head": head,
            "matches": head == pin.get("commit", ""),
            "patches": patches,
            "patched": all(patch_is_applied(directory, patch) for patch in patches),
            "tag": pin.get("tag", ""),
            "purpose": pin.get("purpose", ""),
        }
    return report


def command_deps(args: argparse.Namespace) -> int:
    if args.action == "status":
        step("Dependency status")
        report = dependency_report()
        matches = 0
        for name, info in report.items():
            if info["matches"]:
                patches = info.get("patches") or []
                note = ""
                if patches:
                    note = f"  + {len(patches)} local patch"
                    if not info.get("patched"):
                        note = "  (local patch missing)"
                ok(f"{name:<12} {info['commit'][:12]}  ({info['tag']}){note}")
                matches += 1
                if info.get("patches") and not info.get("patched"):
                    warn(f"{name:<12} local patches not applied; run: python tools/wp.py deps fetch")
            elif info["present"]:
                warn(f"{name:<12} at {str(info['head'])[:12]}, lockfile wants {info['commit'][:12]}")
            else:
                fail(f"{name:<12} missing (pinned to {info['commit'][:12]})")
        say("")
        say(f"{matches}/{len(report)} dependencies match the lockfile")
        return 0 if matches == len(report) else 1

    # deps fetch
    step("Fetching pinned dependencies into external/")
    require_tools("git")
    report = dependency_report()
    fetched = 0
    skipped = 0
    for name, pin in load_lockfile().items():
        commit = pin.get("commit", "")
        repository = pin.get("repository", "")
        directory = EXTERNAL_DIR / name
        if report.get(name, {}).get("matches"):
            say(f"  {STYLE.dim}{name}: already at {commit[:12]}{STYLE.end}")
            apply_dependency_patches(name, directory)
            skipped += 1
            continue
        if not commit or not repository:
            die(f"{name}: lockfile entry is missing 'repository' or 'commit'")
        if directory.exists():
            shutil.rmtree(directory, ignore_errors=True)
        directory.parent.mkdir(parents=True, exist_ok=True)
        code = run(["git", "init", "--quiet", str(directory)])
        if code != 0:
            die(f"git init failed for {name}")
        code = run(["git", "-C", str(directory), "remote", "add", "origin", repository])
        if code != 0:
            die(f"git remote add failed for {name}")
        say(f"  fetching {name} @ {commit[:12]}")
        code = run(
            ["git", "-C", str(directory), "fetch", "--quiet", "--depth", "1", "origin", commit]
        )
        if code != 0:
            # Some servers refuse fetching by sha; fall back to a full fetch.
            code = run(["git", "-C", str(directory), "fetch", "--quiet", "origin"])
        if code != 0:
            die(f"could not fetch {name} from {repository}")
        if run(["git", "-C", str(directory), "checkout", "--quiet", commit]) != 0:
            die(f"could not check out {commit} for {name}")
        apply_dependency_patches(name, directory)
        fetched += 1

    say("")
    say(f"fetched {fetched}, already present {skipped}, total {fetched + skipped}")
    return 0


def command_new(args: argparse.Namespace) -> int:
    name = args.name.strip()
    if not name:
        die("project name cannot be empty")
    if not re.fullmatch(r"[A-Za-z0-9_\- ]+", name):
        die("project name may only contain letters, numbers, spaces, '-' and '_'")

    destination = Path(args.directory).expanduser() if args.directory else Path.cwd() / name
    destination = destination.resolve()
    if destination.exists() and any(destination.iterdir()):
        die(f"{destination} already exists and is not empty")

    source = TEMPLATES_DIR / "blank_project"
    if not source.is_dir():
        die(f"missing project template: {source}")

    step(f"Creating project '{name}' in {destination}")
    shutil.copytree(source, destination, dirs_exist_ok=True)

    # Replace the placeholder in every text file the template ships with.
    for path in destination.rglob("*"):
        if path.is_file() and path.suffix.lower() in {".json", ".lua", ".md", ".txt"}:
            try:
                text = path.read_text(encoding="utf-8")
            except UnicodeDecodeError:
                continue
            if NAME_TOKEN in text or "BlankProject" in text:
                path.write_text(
                    text.replace(NAME_TOKEN, name).replace("BlankProject", name),
                    encoding="utf-8",
                )

    project_manifest(destination)
    ok(f"project ready: {destination}")
    say("")
    say("Next steps:")
    say(f"  python tools/wp.py run {destination}")
    say(f"  python tools/wp.py export {destination}")
    return 0


def command_build(args: argparse.Namespace) -> int:
    preset = preset_name(release=args.release, editor=args.editor)
    if preset == "headless" and not args.quiet_headless_note:
        warn("WAYPOINT_HEADLESS_BUILD is set: building tests only (no window, no runtime)")
    if args.target:
        step(f"Building target '{args.target}' (preset {preset})")
    else:
        step(f"Building Waypoint Engine (preset {preset})")
    build(preset, args.target)
    return 0


def command_test(args: argparse.Namespace) -> int:
    preset = preset_name(headless=True if headless_requested() else None)
    tools = require_tools("cmake", "ninja", "ctest")
    step(f"Testing (preset {preset})")
    build(preset, target="waypoint_tests")
    command = [
        tools["ctest"],
        "--test-dir",
        str(build_dir_for(preset)),
        "--output-on-failure",
    ]
    if run(command, cwd=REPO_ROOT) != 0:
        die("tests failed")
    ok("all tests passed")
    return 0


def command_run(args: argparse.Namespace) -> int:
    project_dir = resolve_project(args.project)
    manifest = project_manifest(project_dir)

    preset = preset_name(release=args.release)
    if preset == "headless":
        die(
            "WAYPOINT_HEADLESS_BUILD is set, so there is no window to show.\n"
            "  Unset it to run a game."
        )

    binary = find_runtime_binary(preset)
    if not binary:
        step("Runtime not built yet, building it")
        build(preset, target="waypoint")
        binary = find_runtime_binary(preset)
    if not binary:
        die(f"could not find the waypoint executable in {build_dir_for(preset)}")

    arguments = [str(binary), str(project_dir)]
    environment = None
    if args.smoke_test:
        # Prove the frame loop on a machine with no display: bgfx's noop backend
        # runs the real submit path (create programs, issue draws, present) and
        # SDL's dummy video driver supplies a window without an X server.
        arguments.append("--smoke-test")
        environment = dict(os.environ)
        environment.setdefault("WAYPOINT_RENDERER", "noop")
        environment.setdefault("SDL_VIDEODRIVER", "dummy")

    step(f"Running '{manifest.get('name', project_dir.name)}'")
    say(f"{STYLE.dim}$ {' '.join(arguments)}{STYLE.end}")
    return run(arguments, cwd=binary.parent, env=environment)


def platform_tag() -> str:
    system = platform.system().lower()
    machine = platform.machine().lower()
    machine = {"x86_64": "x64", "amd64": "x64", "aarch64": "arm64", "arm64": "arm64"}.get(
        machine, machine
    )
    if system == "darwin":
        return f"macos-{machine}"
    if system == "windows":
        return f"windows-{machine}"
    return f"linux-{machine}"


def command_export(args: argparse.Namespace) -> int:
    if headless_requested():
        die(
            "export is not available in a headless build (WAYPOINT_HEADLESS_BUILD=1).\n"
            "  A package needs a real runtime binary, which needs a window system."
        )

    project_dir = resolve_project(args.project)
    manifest = project_manifest(project_dir)
    name = str(manifest.get("name") or project_dir.name)

    preset = preset_name(release=True if args.release else None)
    step(f"Building a {'release' if args.release else 'default'} runtime for export")
    build(preset, target="waypoint")

    binary = find_runtime_binary(preset)
    if not binary:
        die(f"could not find the waypoint executable in {build_dir_for(preset)}")

    tag = platform_tag()
    safe_name = re.sub(r"[^A-Za-z0-9_\-]+", "-", name).strip("-") or "game"
    destination = DIST_ROOT / f"{safe_name}-{tag}"
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True, exist_ok=True)

    executable_name = f"{safe_name}{EXE_SUFFIX}"
    shutil.copy2(binary, destination / executable_name)

    # Shaders live next to the built runtime; copy whatever is there.
    shaders_source = binary.parent / "shaders"
    if not shaders_source.is_dir():
        shaders_source = build_dir_for(preset) / "shaders"
    if shaders_source.is_dir():
        shutil.copytree(shaders_source, destination / "shaders", dirs_exist_ok=True)
    else:
        warn("no compiled shaders found; the game will show its fallback screen")

    # Game content becomes ./game next to the executable.
    shutil.copytree(
        project_dir,
        destination / "game",
        dirs_exist_ok=True,
        ignore=shutil.ignore_patterns("build", "dist", ".git", "__pycache__", "*.pyc"),
    )

    if IS_WINDOWS:
        (destination / "run.bat").write_text(
            "@echo off\r\n"
            "cd /d \"%~dp0\"\r\n"
            f'"{executable_name}" game\r\n',
            encoding="utf-8",
        )
        (destination / "run.sh").write_text(
            "#!/bin/sh\ncd \"$(dirname \"$0\")\"\n"
            f'exec wine "./{executable_name}" game "$@"\n',
            encoding="utf-8",
        )
    else:
        run_script = destination / "run.sh"
        run_script.write_text(
            "#!/bin/sh\n"
            "cd \"$(dirname \"$0\")\"\n"
            f'exec "./{executable_name}" game "$@"\n',
            encoding="utf-8",
        )
        run_script.chmod(0o755)

    (destination / "README.txt").write_text(
        f"{name} - made with Waypoint Engine {manifest.get('engineVersion', '')}\n\n"
        "How to play:\n"
        "  Linux/macOS: ./run.sh\n"
        "  Windows:     run.bat\n\n"
        "The game content is in the 'game' folder and can be edited with any text editor.\n",
        encoding="utf-8",
    )

    ok(f"exported to {destination}")
    return 0


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="wp.py",
        description="Waypoint Engine helper: build, run, test, export and manage projects.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "examples:\n"
            "  python tools/wp.py doctor\n"
            "  python tools/wp.py new MyGame\n"
            "  python tools/wp.py build --release\n"
            "  python tools/wp.py run MyGame\n"
            "  WAYPOINT_HEADLESS_BUILD=1 python tools/wp.py test\n"
        ),
    )
    sub = parser.add_subparsers(dest="command", metavar="command")

    doctor = sub.add_parser("doctor", help="check that this machine can build Waypoint")
    doctor.set_defaults(func=command_doctor)

    new = sub.add_parser("new", help="create a new project from the blank template")
    new.add_argument("name", help="project name, e.g. MyGame")
    new.add_argument("--directory", help="where to create it (default: ./<name>)")
    new.set_defaults(func=command_new)

    build_cmd = sub.add_parser("build", help="configure and build the engine")
    build_cmd.add_argument("--release", action="store_true", help="optimised release build")
    build_cmd.add_argument("--editor", action="store_true", help="also build the Qt editor")
    build_cmd.add_argument("--target", help="build only this CMake target")
    build_cmd.set_defaults(func=command_build, quiet_headless_note=False)

    run_cmd = sub.add_parser("run", help="run a project in the waypoint player")
    run_cmd.add_argument("project", help="project folder or name")
    run_cmd.add_argument("--release", action="store_true", help="use the release runtime")
    run_cmd.add_argument(
        "--smoke-test",
        action="store_true",
        help="render one frame with the noop backend and exit (used by CI)",
    )
    run_cmd.set_defaults(func=command_run)

    test_cmd = sub.add_parser("test", help="build and run the unit tests")
    test_cmd.set_defaults(func=command_test)

    export_cmd = sub.add_parser("export", help="package a project into dist/")
    export_cmd.add_argument("project", help="project folder or name")
    export_cmd.add_argument("--release", action="store_true", help="use a release runtime")
    export_cmd.set_defaults(func=command_export)

    deps = sub.add_parser("deps", help="inspect or fetch pinned dependencies")
    deps.add_argument("action", choices=["status", "fetch"])
    deps.set_defaults(func=command_deps)

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if not getattr(args, "command", None):
        parser.print_help()
        return 0
    return int(args.func(args) or 0)


if __name__ == "__main__":
    sys.exit(main())
