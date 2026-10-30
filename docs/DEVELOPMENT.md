# EdgeZoom 0.5.1 development notes

## Rendering

Windows Graphics Capture supplies live frames to a Direct3D 11 flip-model swap chain with two buffers. A complete frame remains visible until another frame is presented. If the desktop has not changed, a viewport change can still be drawn using the most recent complete capture. The repeatedly repainted native Magnification control is no longer used.

Live capture is isolated in `src/capture.hpp` and `src/capture.cpp`; GPU scaling and presentation remain in `src/renderer.hpp` and `src/renderer.cpp`. The rest of the application remains Win32 C++17. No browser engine or third-party application framework is used. Desktop images stay on the GPU during normal operation.

Desktop Duplication can bake the pointer into the captured image on some display paths, which caused a second magnified pointer despite the application never drawing one. Version 0.5.1 uses `IGraphicsCaptureSession2::put_IsCursorCaptureEnabled(false)` instead. It reads the property back and requires confirmation that cursor inclusion is disabled before starting capture. There is no cursor-inclusive fallback. Windows continues to draw the normal system pointer; the app neither hides it nor draws a replacement. The free-threaded frame pool needs no additional UI dispatcher. Each frame is closed after its GPU copy, and the session/pool are closed on exit.

The overlay is marked `WDA_EXCLUDEFROMCAPTURE` before capture starts, preventing recursive capture without hiding and showing the window. This and the cursor-exclusion setting require Windows 10 2004 or newer. Other screenshot/recording tools may consequently omit the zoom overlay. Windows can show its capture indicator. Device/display capture errors dismiss zoom instead of silently substituting a frozen screenshot. Protected or secure-desktop content is not supported. HDR fidelity and rotated-monitor behavior have not been verified on physical hardware.

## Input and tray

Holding the configured activation button and scrolling up starts zoom. Releasing it preserves live zoom and edge panning. Hold it again to adjust with the wheel; return to 100% or press Escape to dismiss zoom. The default is Alt; pressing Alt without scrolling retains its usual behavior. The mouse-wheel hook does no capture or rendering work: it posts a message to the app's event loop. Raw mouse input is registered only during inspection.

The edge-push rule requires outward relative mouse motion after the pointer has arrived at a boundary. A stationary pointer never pans. Pan offsets are original-desktop pixels and are no longer divided by the zoom scale. Tests check the same displacement at 2x, 4x, 8x and 16x. Raw movement is independent of Windows pointer acceleration; custom pointer-speed/acceleration settings can affect how closely its physical feel matches the desktop pointer.

The prior cursor clipping rectangle is restored when zoom is dismissed. The overlay is for inspection; press Escape or return to 100% before interacting with other apps.

A notification-area icon remains registered while the process runs. Windows may put it in the hidden-icons overflow. Left-click opens settings; right-click exposes Settings, Pause/Resume and Quit. Settings also has Pause and Quit buttons. Ctrl+Alt+Q quits when registration of that shortcut is available. If the tray cannot be registered, settings stays accessible instead of disappearing into the background. Restarting Explorer re-registers the tray icon.

## Build

See the repository README for Visual Studio/CMake and LLVM-MinGW commands. MSVC builds use a static C++ runtime. The portable build uses LLVM-MinGW 20261020, UCRT x64, optimized C++17 and a static C++ runtime. Windows supplies D3D11, DXGI, D3DCompiler, DWM, User32, GDI32, Shell32 and the Windows Runtime. The executable is unsigned and requests neither administrator nor UIAccess privileges.

## Tests

`tests/view_test.cpp` checks anchored zoom, boundary arrival, resting at boundaries, all four outward directions, inward movement, source bounds, zoom limits, no drift, equal original-desktop displacement at different scales, and 1,000 mixed operations. Assertions remain enabled in Release builds. CI runs these tests without an interactive desktop.

Quit any existing instance before running `EdgeZoom.exe --smoke-test` locally. The test creates an independent synthetic source window, magnifies it, changes its content, and samples rendered output repeatedly while panning. Samples are taken from a 1-pixel GPU staging texture copied immediately before Present, avoiding a false reading from a different back buffer after a swap. This diagnostic readback is absent from ordinary use. It also verifies persistent zoom without a held modifier, return to 100%, restored clipping, and tray registration. Normal mouse movement/wheel input is isolated from the scripted test; Escape can still cancel it.

The test also requires the capture session's cursor-inclusion property to read back as false. It writes `EdgeZoom.ini.test.txt` beside the executable, containing pass/fail information, synthetic pixel values, frame-sample counts, tray registration and cursor-exclusion status, not screenshots or input history. Exit 0 means pass, 2/3 failure, and 4 means another instance prevented the test. `--demo` opens the animated synthetic source for manual inspection.

The repeated-frame test improves on the two isolated pixels checked in version 0.3, but it cannot prove that the physical display never flashes. Test the actual monitor and mouse as well, particularly with unusual drivers, HDR or multiple displays.

## Privacy and packaging

No networking, telemetry, updater, account, credential access, keyboard logging, screenshot saving, certificate installation, or automatic startup is implemented. The normal persistent preference is `EdgeZoom.ini` beside the executable. Diagnostic reports are created only by an explicit test run.

`dist/` contains the portable ZIP and checksums. MIT license text is preserved from this repository; runtime notices are included separately. No Wind code is included.
