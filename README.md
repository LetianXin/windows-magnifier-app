# EdgeZoom — Windows Magnifier

A small native Windows app for **live screen zoom with modifier-controlled adjustment**.

**[Download EdgeZoom 0.6.0 for Windows x64](https://github.com/LetianXin/windows-magnifier-app/raw/refs/heads/main/dist/EdgeZoom-v0.6.0-windows-x64.zip)**

Unzip and run `EdgeZoom.exe`. No installation or administrator access is required. The executable is unsigned. [SHA-256 checksums](dist/SHA256SUMS.txt) are provided for the ZIP and its executable.

## How to use

1. Launch the app, then choose **Done - keep in tray**.
2. Hold **Alt** and scroll up to zoom under your pointer. Scroll down to zoom out.
3. Push the mouse outward at a screen edge to pan in original-desktop pixels. Speed is not divided by the zoom level. Merely touching or resting at the edge does nothing.
4. Release Alt: the zoom stays as it is, and you can keep panning.
5. Hold Alt again to adjust with the wheel. Scroll down to 100% to return to normal use, or press Escape.

Alt only enables wheel zooming; pressing or releasing it does not toggle the view. The desktop remains live while you inspect it. There is one pointer at the normal Windows size, no automatic edge scrolling, and no inertial movement. Zoom ranges from 1x to 16x.

The small magnifying-glass icon stays in the Windows notification area while the app is running (Windows may place it under the **Show hidden icons** arrow). Click it to customize the activation button. Options include Alt, Ctrl, Shift, mouse side buttons, Space, Caps Lock, and F1-F12. Right-click for **Pause / Resume** or **Quit EdgeZoom**. **Ctrl + Alt + Q** also quits, unless another app already owns that shortcut. The settings window also has Pause and Quit buttons. The app does not automatically start with Windows.

## What it does

- Live magnification on the monitor under the pointer.
- Pointer-anchored wheel zoom and movement-driven edge panning.
- Customizable hold button, saved beside the executable.
- Portable native C++17 app using Win32, Windows Graphics Capture, and a small Direct3D 11 renderer. Cursor capture is explicitly disabled. The current Windows cursor shape is displayed once at normal size, while the real pointer is mapped to the underlying control.

Click, drag, type, and scroll normally while magnified. The overlay passes mouse input through and keeps keyboard focus in the underlying app. Alt + vertical wheel adjusts zoom; ordinary vertical and horizontal wheel input reaches the app. There are currently no screenshot export, annotation, or recording tools. Pen/touch, games that lock or recenter the pointer, and elevated apps from an unelevated process are not supported. Protected content and secure desktop screens may not appear; HDR color management is not provided.

Requires Windows 10 version 2004 or newer / Windows 11 **x64**, and a compatible display driver. The renderer uses a two-buffer DirectComposition swap chain, limits queued presentation to one frame, and consumes the newest capture frame. GPU resources are prepared at launch without capturing the desktop. Capture starts asynchronously when zoom is requested. The overlay is excluded from capture to prevent recursion. Windows may show its capture indicator while zoom is active. The app does not use the repeatedly repainted Windows magnifier control from version 0.3.

## Privacy

No accounts, networking, telemetry, uploads, updater, certificate installation, or administrator requirement. No screenshots or keystrokes are recorded. A dedicated input-hook thread handles zoom, pointer mapping and Escape. No input events are saved. Normal operation synthesizes only pointer movement; Windows delivers the original buttons, wheel and text to the underlying app. Its normal persistent setting is `EdgeZoom.ini` beside the executable.

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

For a local live-rendering integration test, quit any existing instance, then run the executable with `--smoke-test`. It checks changing rendered pixels across repeated frames while panning, persistent zoom without a held modifier, return to 100%, restored cursor boundaries, and tray registration. It also tests mapped clicks, dragging, all mouse buttons, both wheel axes and typing, and measures startup and live-update timing. Leave input untouched for the five-second test; focus changes safely prevent test input injection. Use `--demo` for an animated test source. See [development notes](docs/DEVELOPMENT.md) for test details and limitations.

## Source layout

| Path | Purpose |
| --- | --- |
| `src/app.cpp` | Tray, settings, activation and input handling |
| `src/input.*` | Dedicated hook thread and native pointer mapping |
| `src/pointer.hpp` | Single normal-size cursor visual and restoration |
| `src/view.hpp` | Zoom bounds, pointer anchoring and edge-push math |
| `src/capture.*` | Live Windows capture with cursor inclusion disabled |
| `src/renderer.*` | GPU scaling and double-buffered presentation |
| `tests/` | Movement tests and live-display test source |
| `dist/` | Tested portable Windows build and checksums |

MIT licensed. Independently written; no Wind source code is included. See [third-party notices](THIRD-PARTY-NOTICES.txt) for the distributed toolchain runtime notices.
