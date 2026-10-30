# EdgeZoom 0.6.0 development notes

## Rendering

Windows Graphics Capture supplies cursor-free frames to a Direct3D 11 / DirectComposition flip-model swap chain. The overlay uses `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE` and never takes keyboard focus. Desktop frames remain on the GPU.

The capture pool is drained with a bounded loop before copying its newest frame. Presentation uses `Present(0, DXGI_PRESENT_DO_NOT_WAIT)`, a frame-latency waitable object, and a maximum queue of one frame. If presentation is busy, rendering yields without blocking the message loop. The pending frame remains dirty until presented. A complete image remains visible between updates.

GPU resources are prepared at launch without desktop capture. Capture starts on zoom and stops at 100%, Escape, Pause or Quit. There is no sleep loop waiting for the first frame. The overlay appears only after a complete frame is ready; startup times out after two seconds. Resources are reused on the same monitor and rebuilt when the monitor or diagnostic mode changes.

The capture session requires `IsCursorCaptureEnabled(false)` and verifies it by readback. The overlay and pointer visual both use `WDA_EXCLUDEFROMCAPTURE`. There is no hide/capture/show loop. Windows may show its capture indicator. Protected content, secure desktops, HDR fidelity and rotated/mixed-DPI display behavior are not verified.

## Input, cursor and tray

`InputRouter` runs mouse and Escape hooks on a dedicated thread, so capture startup, shader compilation, presentation and settings cannot block them. Its mutex protects only viewport math and pointer state; rendering never runs under that mutex. The configured modifier consumes vertical wheel for zoom; releasing it preserves zoom. Original buttons, ordinary wheel events, and keyboard input continue through Windows to the underlying application. Normal operation never synthesizes button clicks or typed text.

Physical mouse movement updates a display-space pointer. The source position is `monitor origin + viewport offset + display pointer / scale`. Tagged absolute `SendInput` movements put the real pointer over the source control without recursively transforming our own injected moves. The hook uses the Windows-accelerated delta observed before the original movement is committed. Outward motion after reaching an edge adds the full delta to source offsets. No timer panning, inertia, or division of pan speed by zoom is used. Windows handles native hit testing, capture, dragging, focus and double clicks; there is no application-specific message forwarding.

The source-position system cursor is hidden with `MagShowSystemCursor`. Its current shape is displayed once at normal size in a small transparent layered window at the display pointer position. Magnification.dll is loaded from System32 solely for cursor visibility; the old Magnification control is not used. Cursor visibility and the prior clipping rectangle are restored on dismissal and errors. Cursor schemes, security settings and certificates are unchanged. Animated cursor frames and unusual inverted cursor schemes may not be reproduced exactly.

The overlay remains active through focus changes so underlying apps stay usable. Escape dismisses zoom and Ctrl+Alt+Q quits. The tray offers Settings, Pause/Resume and Quit and re-registers after Explorer restarts. Injection failure dismisses zoom. Pen/touch, applications that recenter/lock the pointer and elevated targets from an unelevated process are outside current support. No administrator or UIAccess privileges are requested.

## Build

See the repository README for Visual Studio/CMake and LLVM-MinGW commands. MSVC builds use a static C++ runtime. The portable build uses LLVM-MinGW 20261020, UCRT x64, optimized C++17 and a static C++ runtime. Windows supplies D3D11, DXGI, D3DCompiler, DWM, DirectComposition, Magnification (cursor visibility only), User32, GDI32, Shell32 and the Windows Runtime. The executable is unsigned and requests neither administrator nor UIAccess privileges.

## Tests

`tests/view_test.cpp` checks anchored zoom, boundary arrival, resting at boundaries, all four outward directions, inward movement, source bounds, zoom limits, no drift, equal original-desktop displacement at different scales, and 1,000 mixed operations. Assertions remain enabled in Release builds. CI runs these tests without an interactive desktop.

Quit any existing instance before running `EdgeZoom.exe --smoke-test` locally. The test creates an independent synthetic source window, magnifies it, changes its content, and samples rendered output repeatedly while panning. Samples are taken from a 1-pixel GPU staging texture copied immediately before Present, avoiding a false reading from a different back buffer after a swap. This diagnostic readback is absent from ordinary use. It also verifies persistent zoom without a held modifier, return to 100%, restored clipping, and tray registration. Modifier zoom is disabled in the scripted test; Escape can cancel it. Leave mouse and keyboard untouched during its five-second run.

The fixture also receives real generated Windows input through the overlay: a left-button drag, right/middle/side buttons, both wheel axes, and a synthetic `x` character. The harness only injects while its own fixture owns foreground focus. It verifies mapped movement against source coordinates and confirms that focus remains in the fixture. This exercises the actual hook and native delivery, but does not establish compatibility with every third-party app.

The report measures capture-start-to-first-present and source-change-to-rendered-pixel delays, not physical scanout latency. On the development machine the tested change took 187 ms for a cold first frame and 16 ms for a live update, with 175 sampled frames and no unexpected pixels. Hardware and refresh rates affect these numbers.

The test also requires the capture session's cursor-inclusion property to read back as false. It writes `EdgeZoom.ini.test.txt` beside the executable, containing pass/fail information, synthetic pixel values, frame-sample counts, tray registration and cursor-exclusion status, not screenshots or input history. Exit 0 means pass, 2/3 failure, and 4 means another instance prevented the test. `--demo` opens the animated synthetic source for manual inspection.

The repeated-frame test improves on the two isolated pixels checked in version 0.3, but it cannot prove that the physical display never flashes. Test the actual monitor and mouse as well, particularly with unusual drivers, HDR or multiple displays.

## Privacy and packaging

No networking, telemetry, updater, account, credential access, keyboard logging, screenshot saving, certificate installation, or automatic startup is implemented. The normal persistent preference is `EdgeZoom.ini` beside the executable. Diagnostic reports are created only by an explicit test run.

`dist/` contains the portable ZIP and checksums. MIT license text is preserved from this repository; runtime notices are included separately. No Wind code is included.
