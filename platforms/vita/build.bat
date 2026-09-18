@echo off
rem Builds the PlayStation Vita version. VITASDK is where the VitaSDK was unpacked, C:\psvita_dev
rem unless it is set already. Needs cmake and a make on the path (MSYS2's mingw64 has both).
rem
rem   build.bat          a normal build
rem   build.bat clean    throws the build folder away first

if "%VITASDK%"=="" set VITASDK=C:/psvita_dev
if not exist "%VITASDK%/bin/arm-vita-eabi-gcc.exe" (
    echo no VitaSDK in %VITASDK%, set VITASDK to where it was unpacked
    exit /b 1
)
set PATH=%VITASDK%\bin;%PATH%

cd /d "%~dp0"
if "%1"=="clean" rmdir /s /q build 2>nul

cmake -S . -B build -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=%VITASDK%/share/vita.toolchain.cmake || exit /b 1
cmake --build build || exit /b 1

dir /b build\*.vpk
