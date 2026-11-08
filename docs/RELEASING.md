# Releasing

## Version numbers

`0.1.0` style: `MAJOR.MINOR.PATCH`. Before 1.0, breaking changes bump MINOR.
The version lives in:

- `CMakeLists.txt` (`project(WaypointEngine VERSION ...)`)
- `engine/project/Project.hpp` (`kEngineVersion`)
- `tools/wp.py` reads the manifest, so generated projects record it
- `docs/STATUS.md` date line

Keep them in step; a project file that claims the wrong engine version produces a
warning on load, which is the safety net, not the plan.

## Cut a release

1. **Green CI on all three desktop platforms** for `main` (see
   `.github/workflows/build.yml`).
2. Update the version in the files above and add a `CHANGELOG.md` entry.
3. Commit: `Release 0.1.0`.
4. Tag and push:

   ```bash
   git tag -a v0.1.0 -m "Waypoint Engine 0.1.0"
   git push origin v0.1.0
   ```

5. `.github/workflows/release.yml` runs on `v*` tags: builds release binaries for
   Linux, Windows and macOS, packages each with `shaders/`, `game/` and a run
   script, and attaches the archives to a GitHub release.

## What goes in a package

```
waypoint-engine-0.1.0-linux-x64/
  waypoint(.exe)      the player
  shaders/<profile>/  compiled bgfx shaders
  game/               the sample project
  run.sh / run.bat
  README.txt
  LICENSE, NOTICE
```

`python tools/wp.py export <ProjectDir> --release` builds the same layout into
`dist/<name>-<platform>/`. It refuses to run in a headless build, because there
is no runtime binary to ship in that configuration.

## Verifying a package before publishing

```bash
python tools/wp.py export samples/hello_cube --release
cd dist/hello-cube-<platform>
./run.sh
```

Check: the window opens, the grid and cube appear, WASD moves, Escape quits.
If shaders are missing the player shows a readable fallback screen instead of a
black window — treat that as a failed package.

## What we ship and what we do not

- Ship: engine binaries, shaders, sample projects, LICENSE, NOTICE.
- Do not ship: dependency checkouts, `build/`, `.cache/`, editor binaries unless
  the release is explicitly marked as an editor build.

## Checklist

- [ ] CI green on ubuntu / windows / macos
- [ ] `python tools/wp.py test` green locally
- [ ] `docs/STATUS.md` updated (verified vs unverified)
- [ ] `NOTICE` lists every shipped library
- [ ] Version bumped in all four places
- [ ] Tag pushed; release artifacts downloadable and actually launched once
