@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   MayaFlux Windows Distribution Verify
echo ========================================
echo.

echo [1] Checking MayaFluxLib...
if exist "include\MayaFlux\*.hpp" (
    for /f %%i in ('dir /b "include\MayaFlux\*.hpp" 2^>nul ^| find /c /v ""') do (
        echo   OK Headers: %%i files
    )
) else (
    echo   ERROR: No MayaFlux headers found
    set ERROR_LEVEL=1
)
if exist "lib\MayaFluxLib.lib" (echo   OK MayaFluxLib.lib) else (echo   ERROR: Missing MayaFluxLib.lib && set ERROR_LEVEL=1)
if exist "bin\MayaFluxLib.dll" (echo   OK MayaFluxLib.dll) else (echo   ERROR: Missing MayaFluxLib.dll && set ERROR_LEVEL=1)

echo.
echo [2] Checking Lila...
if exist "include\Lila\*.hpp" (
    for /f %%i in ('dir /b "include\Lila\*.hpp" 2^>nul ^| find /c /v ""') do (
        echo   OK Headers: %%i files
    )
) else (
    echo   ERROR: No Lila headers found
    set ERROR_LEVEL=1
)
if exist "lib\Lila.lib"        (echo   OK Lila.lib)        else (echo   ERROR: Missing Lila.lib        && set ERROR_LEVEL=1)
if exist "bin\Lila.dll"        (echo   OK Lila.dll)        else (echo   ERROR: Missing Lila.dll        && set ERROR_LEVEL=1)
if exist "bin\lila_server.exe" (echo   OK lila_server.exe) else (echo   ERROR: Missing lila_server.exe && set ERROR_LEVEL=1)

echo.
echo [3] Checking runtime and shaders...
if exist "share\MayaFlux\runtime\pch.h"   (echo   OK pch.h)    else (echo   ERROR: Missing pch.h    && set ERROR_LEVEL=1)
if exist "share\MayaFlux\.version"         (echo   OK .version) else (echo   WARNING: Missing .version)
set /a spv_count=0
for %%F in (share\MayaFlux\shaders\*.spv) do set /a spv_count+=1
echo   OK SPIR-V shaders: !spv_count!
if !spv_count! LSS 5 (echo   WARNING: Low shader count)

echo.
echo [4] Checking vcpkg dependencies (merged)...
set /a dll_count=0
for %%F in (bin\*.dll) do set /a dll_count+=1
echo   DLLs in bin\: !dll_count!
if !dll_count! LSS 15 (echo   WARNING: Low DLL count)

set /a lib_count=0
for %%F in (lib\*.lib) do set /a lib_count+=1
echo   Libs in lib\: !lib_count!

set REQUIRED_PKGS=glfw3 RtAudio eigen3 glm assimp freetype hidapi utf8proc asio
for %%P in (%REQUIRED_PKGS%) do (
    if exist "share\%%P" (echo   OK %%P) else (echo   WARNING: share\%%P not found)
)

echo.
echo [5] Checking CMake integration...
if exist "lib\cmake\MayaFlux\MayaFluxConfig.cmake"        (echo   OK MayaFluxConfig.cmake)        else (echo   ERROR: Missing MayaFluxConfig.cmake        && set ERROR_LEVEL=1)
if exist "lib\cmake\MayaFlux\MayaFluxConfigVersion.cmake" (echo   OK MayaFluxConfigVersion.cmake) else (echo   ERROR: Missing MayaFluxConfigVersion.cmake && set ERROR_LEVEL=1)

echo.
echo ========================================
if defined ERROR_LEVEL (
    echo VERIFICATION FAILED
    exit /b 1
) else (
    echo VERIFICATION SUCCESSFUL
)
echo ========================================
