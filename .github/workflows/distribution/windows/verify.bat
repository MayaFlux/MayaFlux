@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   MayaFlux Windows Distribution Verify
echo ========================================
echo.

echo [1] Checking MayaFluxLib (Core Engine)...
if exist "include\MayaFlux\*.hpp" (
    for /f %%i in ('dir /b "include\MayaFlux\*.hpp" 2^>nul ^| find /c /v ""') do (
        echo   ✓ Headers: %%i files found
    )
) else (
    echo   ✗ ERROR: No MayaFlux headers found
    set ERROR_LEVEL=1
)

if exist "lib\MayaFluxLib.lib" (
    echo   ✓ Import library: MayaFluxLib.lib
) else (
    echo   ✗ ERROR: Missing MayaFluxLib.lib
    set ERROR_LEVEL=1
)

if exist "bin\MayaFluxLib.dll" (
    for %%F in ("bin\MayaFluxLib.dll") do (
        echo   ✓ Runtime: %%~nxF
    )
) else (
    echo   ✗ ERROR: Missing MayaFluxLib.dll
    set ERROR_LEVEL=1
)

echo.
echo [2] Checking Lila (JIT Interface)...
if exist "include\Lila\*.hpp" (
    for /f %%i in ('dir /b "include\Lila\*.hpp" 2^>nul ^| find /c /v ""') do (
        echo   ✓ Headers: %%i files found
    )
) else (
    echo   ✗ ERROR: No Lila headers found
    set ERROR_LEVEL=1
)

if exist "lib\Lila.lib" (
    echo   ✓ Import library: Lila.lib
) else (
    echo   ✗ ERROR: Missing Lila.lib
    set ERROR_LEVEL=1
)

if exist "bin\Lila.dll" (
    for %%F in ("bin\Lila.dll") do (
        echo   ✓ Runtime: %%~nxF
    )
) else (
    echo   ✗ ERROR: Missing Lila.dll
    set ERROR_LEVEL=1
)

echo.
echo [3] Checking lila_server (Application)...
if exist "bin\lila_server.exe" (
    for %%F in ("bin\lila_server.exe") do (
        set size=%%~zF
        set /a size_mb=!size!/1048576
        echo   ✓ Executable: %%~nxF (!size_mb! MB)
    )
) else (
    echo   ✗ ERROR: Missing lila_server.exe
    set ERROR_LEVEL=1
)

echo.
echo [4] Checking Runtime Dependencies...
set /a dll_count=0
for %%F in (bin\*.dll) do set /a dll_count+=1
echo   ✓ Found !dll_count! runtime DLLs

if !dll_count! LSS 10 (
    echo   ⚠ WARNING: Low DLL count, some dependencies may be missing
)

echo.
echo ========================================
if defined ERROR_LEVEL (
    echo ❌ VERIFICATION FAILED
    exit /b 1
) else (
    echo ✅ VERIFICATION SUCCESSFUL
)
echo ========================================
