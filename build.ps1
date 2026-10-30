param([string]$CompilerRoot)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
 if (-not $CompilerRoot) { throw 'Pass -CompilerRoot pointing to an LLVM-MinGW installation, or use CMake with Visual Studio.' }
 $bin=Join-Path $CompilerRoot 'bin'
 New-Item -ItemType Directory -Path build -Force | Out-Null
 & "$bin\llvm-windres.exe" -i app.rc -o build\resources.o
 if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed' }
 & "$bin\x86_64-w64-mingw32-clang++.exe" -std=c++17 -O2 -Wall -Wextra -Wno-missing-field-initializers -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -municode -mwindows -static -s src\app.cpp src\renderer.cpp src\capture.cpp build\resources.o -o build\EdgeZoom.exe -ld3d11 -ldxgi -ld3dcompiler -ldwmapi -luser32 -lgdi32 -lshell32 -lruntimeobject
 if ($LASTEXITCODE -ne 0) { throw 'Application build failed' }
 & "$bin\x86_64-w64-mingw32-clang++.exe" -std=c++17 -O2 -Wall -Wextra -static tests\view_test.cpp -o build\view_test.exe
 if ($LASTEXITCODE -ne 0) { throw 'Math test build failed' }
 & '.\build\view_test.exe'
 if ($LASTEXITCODE -ne 0) { throw 'Math tests failed' }
 Get-Item build\EdgeZoom.exe | Select-Object FullName,Length
} finally { Pop-Location }
