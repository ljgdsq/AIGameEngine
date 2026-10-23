@echo off
setlocal EnableExtensions
title Aster Engine
cd /d "%~dp0"

rem Find the newest editor build without requiring a specific generator layout.
set "EDITOR="
for %%B in (build-aster build) do (
    for %%C in (Debug Release) do (
        if not defined EDITOR if exist "%%B\bin\%%C\AsterEditor.exe" set "EDITOR=%%B\bin\%%C\AsterEditor.exe"
    )
)

if not defined EDITOR (
    echo [Aster] AsterEditor.exe was not found.
    echo.
    echo Build the editor first:
    echo   cmake -S . -B build-aster -DASTER_BUILD_TESTS=ON
    echo   cmake --build build-aster --config Debug --target AsterEditor
    echo.
    pause
    exit /b 1
)

rem Split arguments: a directory (or project.aster.yaml) becomes --project,
rem while flags such as --play, --frames, and --screenshot are passed through.
set "PROJECT="
set "PASSTHRU="
:parse
if "%~1"=="" goto launch
if not defined PROJECT (
    if exist "%~1\" (
        set "PROJECT=%~1"
        shift
        goto parse
    )
    if /I "%~nx1"=="project.aster.yaml" (
        set "PROJECT=%~dp1"
        shift
        goto parse
    )
)
set "PASSTHRU=%PASSTHRU% "%~1""
shift
goto parse

:launch
echo [Aster] Editor: %EDITOR%
if defined PROJECT echo [Aster] Project: %PROJECT%
echo.

if defined PROJECT (
    "%EDITOR%" --project "%PROJECT%" %PASSTHRU%
) else (
    "%EDITOR%" %PASSTHRU%
)

set "EXIT_CODE=%ERRORLEVEL%"
if not "%EXIT_CODE%"=="0" (
    echo.
    echo [Aster] Editor exited with code %EXIT_CODE%.
    pause
)
endlocal & exit /b %EXIT_CODE%
