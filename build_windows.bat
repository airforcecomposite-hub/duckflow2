@echo off
setlocal
echo ============================================================
echo   DuckFlow 2 - Windows x64 VST3 Automatic Build Script
echo ============================================================

where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake was not found. Install CMake from https://cmake.org/download/ or Visual Studio Installer.
    pause
    exit /b 1
)

echo [1/2] Configuring CMake and fetching JUCE 8...
cmake -B build -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed. Make sure Visual Studio 2022 C++ workload and Git are installed.
    pause
    exit /b 1
)

echo [2/2] Building DuckFlow 2.vst3 (Release)...
cmake --build build --config Release --parallel
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed.
    pause
    exit /b 1
)

echo.
echo [SUCCESS] Build complete!
echo Your VST3 bundle is ready at:
echo   build\DuckFlow2_artefacts\Release\VST3\DuckFlow 2.vst3
echo.
echo Copy "DuckFlow 2.vst3" into:
echo   C:\Program Files\Common Files\VST3\
echo.
pause
