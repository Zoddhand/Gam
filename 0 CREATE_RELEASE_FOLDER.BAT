@echo off
setlocal enabledelayedexpansion

REM =========================
REM SDL3 Release Packager
REM =========================
REM Assumptions (per your message):
REM - This .bat is in your project root
REM - DLLs are in the SAME folder as this .bat
REM - ASSETS\ and maps\ are in the SAME folder as this .bat
REM - EXE is at x64\Release\gam.exe
REM Output goes to: dist\Release\

set "ROOT=%~dp0"
set "EXE_SRC=%ROOT%x64\Release\gam.exe"

set "OUT_DIR=%ROOT%dist\Release"
set "EXE_DST=%OUT_DIR%\gam.exe"

REM DLLs next to the .bat
set "DLL_SRC_DIR=%ROOT%"
set "DLL1=SDL3.DLL"
set "DLL2=SDL3_IMAGE.DLL"
set "DLL3=SDL2_MIXER.DLL"
set "DLL4=SDL3_TTF.DLL"

echo.
echo === Packaging Release ===
echo Root   : "%ROOT%"
echo Exe    : "%EXE_SRC%"
echo Output : "%OUT_DIR%"
echo.

REM ---- sanity check EXE ----
if not exist "%EXE_SRC%" (
  echo ERROR: Missing EXE: "%EXE_SRC%"
  exit /b 1
)

REM ---- clean output ----
if exist "%OUT_DIR%" (
  rmdir /s /q "%OUT_DIR%"
)
mkdir "%OUT_DIR%" >nul

REM ---- copy EXE ----
echo Copying EXE...
copy /y "%EXE_SRC%" "%EXE_DST%" >nul
if errorlevel 1 (
  echo ERROR: Failed to copy EXE.
  exit /b 1
)

REM ---- copy DLLs from root (same folder as .bat) ----
echo Copying DLLs...
for %%D in ("%DLL1%" "%DLL2%" "%DLL3%" "%DLL4%") do (
  if exist "%DLL_SRC_DIR%\%%~D" (
    copy /y "%DLL_SRC_DIR%\%%~D" "%OUT_DIR%\%%~D" >nul
  ) else (
    echo ERROR: Missing DLL next to .bat: "%DLL_SRC_DIR%\%%~D"
    exit /b 1
  )
)

REM ---- copy ASSETS ----
if exist "%ROOT%ASSETS" (
  echo Copying ASSETS\ ...
  xcopy "%ROOT%ASSETS" "%OUT_DIR%\ASSETS\" /E /I /Y >nul
) else (
  echo WARNING: ASSETS folder not found next to .bat: "%ROOT%ASSETS"
)

REM ---- copy maps ----
if exist "%ROOT%maps" (
  echo Copying maps\ ...
  xcopy "%ROOT%maps" "%OUT_DIR%\maps\" /E /I /Y >nul
) else (
  echo WARNING: maps folder not found next to .bat: "%ROOT%maps"
)

echo.
echo DONE. Run from:
echo   "%OUT_DIR%\gam.exe"
echo.
pause
endlocal
