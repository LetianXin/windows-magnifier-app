# EdgeZoom 0.3 — live zoom

A small, portable Windows screen inspector. Original C++17 code using Win32 and the built-in Windows Magnification control. No custom Direct3D renderer, browser engine, installer, or external runtime download.

## Use

1. Run `EdgeZoom.exe` and click **Done - keep in tray**.
2. Hold **Alt** and scroll up over the area you want to inspect. Scroll down to zoom out. Zoom ranges from 1x to 16x and anchors at the pointer.
3. Move the pointer normally. When it reaches a screen edge, continue pushing outward to pan. Merely reaching or resting at an edge does nothing. Panning stops as soon as outward movement stops.
4. Release Alt to return to normal use. Escape also dismisses zoom.

Click the tray icon to change the activation button. Choices include Alt, Ctrl, Shift, mouse side buttons, Space, Caps Lock, and F1-F12. Changes are saved in `EdgeZoom.ini` beside the executable. Alt is the default. The button's usual action is not globally disabled; choose a modifier to minimize conflicts with other applications.

There is no Ctrl+Alt+Z toggle and no onscreen toolbar. Starting the executable again opens settings. Right-click its tray icon and select Quit to exit. `--tray` starts with settings hidden. The app does not add itself to Windows startup.

## Behavior and limits

- The desktop stays live while zoom is active, including changing content in the inspected region. The native control refreshes on a 16 ms timer even when the pointer and viewport are stationary. Actual frame rate depends on Windows and the display.
- Only the normal Windows pointer is shown. The native control's magnified-cursor style is deliberately disabled and the app draws no extra pointer.
- The overlay is for inspection. Release the activation button before clicking or typing into the underlying application.
- Uses the monitor under the pointer. The pointer remains within that monitor while inspecting; the prior cursor boundary is restored on exit. Screen-layout changes and loss of focus dismiss the overlay.
- Windows handles image scaling. There is no zoom easing or inertial panning.
- Edge pushing uses relative raw mouse movement. Absolute-position tablets are not supported for edge pushing; physical mouse and touchpad behavior can differ.
- Intended for Windows 10/11 x64 with a WDDM display driver. Protected content or secure desktop screens may not appear. The app does not provide HDR color management. If live magnification cannot start, it reports an error instead of substituting a frozen image.

## Structure and privacy

- `src/app.cpp`: native tray/settings windows, wheel activation, hold release, and raw mouse input.
- `src/view.hpp`: bounded, pointer-anchored zoom and movement-driven edge-push math.
- `src/renderer.cpp` and `.hpp`: a native magnifier child control, a scale matrix, and a desktop source rectangle. Its host is excluded from magnification to prevent recursive capture. The system Magnification DLL is loaded only from the Windows system directory.
- `tests/live_fixture.hpp`: an animated source window used only by the explicit demo and smoke-test flags.

The process uses a Windows low-level mouse hook to intercept the wheel only during the configured activation gesture. It checks the selected key's held state; it does not install a keyboard hook or record keystrokes. Raw mouse input is registered only during inspection. No screenshots are saved, no network APIs are used by the application, and there is no updater, telemetry, account, certificate installation, UIAccess privilege, or administrator requirement. The only normal persistent file is the chosen activation-key setting. The executable is unsigned.

Windows handles accelerated capture and composition inside its own magnifier control; the application itself does not manage a GPU. While idle in the tray, no magnifier is running and no screen-refresh timer is active.

## Build

Using an existing LLVM-MinGW toolchain:

```powershell
.\build.ps1 -CompilerRoot 'C:\path\to\llvm-mingw'
```

Or use Visual Studio's C++ tools and CMake:

```powershell
cmake -S . -B build-msvc -A x64
cmake --build build-msvc --config Release
ctest --test-dir build-msvc -C Release --output-on-failure
```

The delivered binary was built with LLVM-MinGW 20261020, UCRT x64, with optimization and a static C++ runtime. It imports only Windows system DLLs. Toolchain license information is included in `THIRD-PARTY-NOTICES.txt`.

## Verification

- Math tests cover cursor anchoring, edge arrival without panning, stationary edges, all four outward directions, moving inward, proportional movement, no drift, source bounds, scale limits, and 1,000 mixed zoom/pan operations. Assertions remain enabled in Release builds.
- `EdgeZoom.exe --smoke-test` creates an underlying test window with spatially distinct regions, verifies a magnified displayed pixel, changes only the underlying content with the viewport stationary, and verifies the new displayed pixel. The test source is kept above unrelated windows and below the zoom overlay. It then checks a zoom change, activation release handling, and cursor-boundary restoration. This tests actual live display, not just successful API calls. Exit 0 means success; 2 or 3 means failure; 4 means an existing instance prevented the test. Quit any existing instance first and do not hold the activation button during the test. An explicit test run writes its result and two synthetic pixel values to `EdgeZoom.ini.test.txt` beside the executable; ordinary use does not create this report.
- `EdgeZoom.exe --demo` opens the live test source with changing colors and a frame counter; Escape exits it. This diagnostic flag intentionally permits viewing without a held key.
- The live-display integration test and unchanged edge-push math tests passed on the development machine. A sustained physical hold-and-push gesture was not verified by desktop automation; hands-on mouse feel remains to be checked.

This is an independently written app. It contains no Wind source code.
