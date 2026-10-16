@echo off
rem Build + run the unit tests with MSVC environment + clang-cl + Ninja
rem Usage: tools\tests_msvc.bat [Debug | DebugNoSan | Release]

set TYPE=%1
if "%TYPE%"=="" set TYPE=Debug

call "F:\software\VS\product\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
set "PATH=C:\Program Files\LLVM\bin;%PATH%"

set CC=clang-cl

"C:\Program Files\CMake\bin\cmake.exe" -B build -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=%TYPE% || exit /b 1
"C:\Program Files\CMake\bin\cmake.exe" --build build --target tests || exit /b 1

rem The ASan runtime DLL must sit next to the exe (Debug only; Release/DebugNoSan link nothing)
if /i "%TYPE%"=="Debug" copy /y "C:\Program Files\LLVM\lib\clang\22\lib\windows\clang_rt.asan_dynamic-x86_64.dll" build\ >nul 2>&1

build\tests.exe %2