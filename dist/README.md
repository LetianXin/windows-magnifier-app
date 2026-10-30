# Portable build

`EdgeZoom-v0.3.0-windows-x64.zip` contains the tested native Windows x64 executable, usage instructions, MIT license, and runtime notices. The source for this version is checked into this repository.

Built with LLVM-MinGW 20261020 (UCRT x64), optimized C++17 and a static C++ runtime. The executable is unsigned and requires no installer. `SHA256SUMS.txt` includes hashes for the ZIP and its contained executable.

Validation: movement-math tests passed; the interactive test verified two different live magnified pixel values while the viewport stayed fixed, followed by zoom, release and cursor-boundary restoration. Physical mouse feel has not been verified by desktop automation.
