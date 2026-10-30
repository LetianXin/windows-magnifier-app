# Portable build

`EdgeZoom-v0.5.0-windows-x64.zip` contains the tested native Windows x64 executable, usage instructions, MIT license, and runtime notices. The source for this version is checked into this repository.

Built with LLVM-MinGW 20261020 (UCRT x64), optimized C++17 and a static C++ runtime. The executable is unsigned and requires no installer. `SHA256SUMS.txt` includes hashes for the ZIP and its contained executable.

Validation: movement-math tests cover original-desktop panning speed at 2x, 4x, 8x, and 16x. The interactive test checks changing captured/rendered pixels and repeated frames during panning, followed by persistent zoom without a held modifier, return to 100%, cursor-boundary restoration, and tray registration. Render-target samples cannot prove that a monitor never flashes; physical mouse feel and actual display behavior still need hands-on testing.
