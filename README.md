# EdgeZoom — Windows Magnifier

A small native Windows app for **live, hold-to-zoom screen inspection**.

**[Download EdgeZoom 0.3.0 for Windows x64](https://github.com/LetianXin/windows-magnifier-app/raw/refs/heads/main/dist/EdgeZoom-v0.3.0-windows-x64.zip)**

Unzip and run `EdgeZoom.exe`. No installation or administrator access is required. The executable is unsigned. [SHA-256 checksums](dist/SHA256SUMS.txt) are provided for the ZIP and its executable.

## How to use

1. Launch the app, then choose **Done - keep in tray**.
2. Hold **Alt** and scroll up to zoom under your pointer. Scroll down to zoom out.
3. Push the mouse outward at a screen edge to pan. Merely touching or resting at the edge does nothing.
4. Release Alt to return to normal use. Escape also exits zoom.

The desktop remains live while you inspect it. There is one normal Windows pointer, no automatic edge scrolling, and no inertial movement. Zoom ranges from 1x to 16x.

Click the tray icon to customize the activation button. Options include Alt, Ctrl, Shift, mouse side buttons, Space, Caps Lock, and F1-F12. Right-click the tray icon to quit. The app does not automatically start with Windows.

## What it does

- Live magnification on the monitor under the pointer.
- Pointer-anchored wheel zoom and movement-driven edge panning.
- Customizable hold button, saved beside the executable.
- Portable native C++17 app using Win32 and the Windows Magnification control.

The overlay is for inspection: release the activation button before clicking or typing into the underlying app. There are currently no screenshot export, annotation, or recording tools. Absolute-position tablets are not supported for edge pushing. Protected content and secure desktop screens may not appear; HDR color management is not provided.

Requires Windows 10/11 **x64** and a WDDM display driver. The application does not manage a Direct3D renderer; Windows handles live capture and composition through its built-in magnifier control.

## Privacy

No accounts, networking, telemetry, uploads, updater, certificate installation, or administrator requirement. No screenshots or keystrokes are recorded. The app uses a mouse-wheel hook for the activation gesture and raw mouse input while inspecting. Its normal persistent setting is `EdgeZoom.ini` beside the executable.

## Build and test

With Visual Studio C++ tools and CMake:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The app is produced at `build/Release/EdgeZoom.exe`.

Or use an existing LLVM-MinGW toolchain:

```powershell
.\build.ps1 -CompilerRoot 'C:\path\to\llvm-mingw'
```

That build produces `build/EdgeZoom.exe` and runs the movement-math tests. [Windows CI](https://github.com/LetianXin/windows-magnifier-app/actions) builds with MSVC, runs those tests, and uploads an executable artifact. Interactive live-display tests require a normal desktop session and are not run in CI.

For a local live-display integration test, quit any existing instance, then run the executable with `--smoke-test`. It checks actual changing magnified pixels, zoom, release, and restored cursor boundaries. Use `--demo` for an animated test source. See [development notes](docs/DEVELOPMENT.md) for test details and limitations.

## Source layout

| Path | Purpose |
| --- | --- |
| `src/app.cpp` | Tray, settings, activation and input handling |
| `src/view.hpp` | Zoom bounds, pointer anchoring and edge-push math |
| `src/renderer.*` | Native live magnifier control |
| `tests/` | Movement tests and live-display test source |
| `dist/` | Tested portable Windows build and checksums |

MIT licensed. Independently written; no Wind source code is included. See [third-party notices](THIRD-PARTY-NOTICES.txt) for the distributed toolchain runtime notices.
