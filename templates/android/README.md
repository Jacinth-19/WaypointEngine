# Android template (placeholder)

Android is a **later milestone** for Waypoint Engine. This folder is a marker, not
a working Gradle project: nothing here produces an APK yet.

What exists today:

- the engine core, ECS, scene and project layers compile for Android ABIs
  (plain C++20, no platform assumptions);
- Jolt and Lua have no Android-specific requirements;
- SDL3 and bgfx build for Android, but the engine has no activity, no
  `ANativeWindow` hand-off and no touch input mapping yet.

What is missing is written down in [docs/ANDROID.md](../../docs/ANDROID.md),
including the exact pieces needed for a real template.

If you want to prove the C++ side compiles for a phone today:

```bash
cmake -S . -B build/android -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
  -DWAYPOINT_HEADLESS_BUILD=ON
cmake --build build/android
```
