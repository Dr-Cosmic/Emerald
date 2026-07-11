@echo off
REM build.bat - Build Emerald on Windows (Visual Studio or MinGW via CMake).
cd /d "%~dp0.."
cmake -S . -B build
if errorlevel 1 exit /b 1
cmake --build build --config Release
if errorlevel 1 exit /b 1
echo.
echo Built: build\Release\emerald.exe  (or build\emerald.exe with MinGW)
echo Try:   build\Release\emerald.exe examples\hello.em
