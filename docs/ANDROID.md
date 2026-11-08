# Android (planned — not implemented)

Android is a **later milestone**. This page records the plan and the honest
current state so nobody assumes `templates/android` produces an APK today.

## Current state

- `templates/android/` is a placeholder with build instructions, not a working
  Gradle project.
- `.github/workflows/android.yml` is manual (`workflow_dispatch`) and only
  attempts a headless NDK compile of the C++ core. It has never been run.
- The engine has no Android activity, no touch input mapping and no
  `bgfx::PlatformData` path for `ANativeWindow`.

## Why it is not trivial

| Piece | Work needed |
| --- | --- |
| SDL3 | builds for Android, but needs a Java activity and `SDL_main` wiring |
| bgfx | Vulkan or GLES backend; needs the `ANativeWindow` handle and a lifecycle-aware swap chain |
| Jolt | compiles for arm64-v8a with no changes |
| Lua 5.4 | compiles, but needs an `extern "C"` wrapper and no `os.execute` |
| Shaders | GLES profile (`300_es` → `essl`) must be baked into the package |
| Packaging | Gradle + NDK `externalNativeBuild`, ABI filters, asset folder for `game/` |

## Planned shape

```
templates/android/
  settings.gradle
  build.gradle
  app/
    build.gradle
    src/main/AndroidManifest.xml
    src/main/java/.../WaypointActivity.java
    src/main/assets/game/            <- project content
    src/main/jni/CMakeLists.txt      <- includes engine + runtime
```

CMake side: one toolchain file (`$ANDROID_NDK/build/cmake/android.toolchain.cmake`),
`-DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24`, with
`WAYPOINT_ENABLE_RENDER=ON` once the window path exists.

## What you can do today

Build the C++ core for arm64 without a window, which proves the engine itself is
portable:

```bash
cmake -S . -B build/android -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
  -DWAYPOINT_HEADLESS_BUILD=ON
cmake --build build/android
```

A PR that lands a real Gradle template and a touch-input mapping is welcome; it
must include an honest status update in `docs/STATUS.md`.
