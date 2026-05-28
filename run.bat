@echo off
setlocal

set SGDK=C:\SGDK
set BLASTEM=D:\emulators\sega mega drive 2\blastem-win32-0.6.2\blastem.exe
set PROJECT=C:\Dev\md-beatemup
set ROM=%PROJECT%\out\rom.bin

cd /d "%PROJECT%"

echo Removing old build output...
if exist "%PROJECT%\out" rmdir /s /q "%PROJECT%\out"

echo Checking for broken duplicate boot source...
if exist "%PROJECT%\src\boot\rom_head.c" (
    findstr /C:"drawDebugHud" "%PROJECT%\src\boot\rom_head.c" >nul
    if not errorlevel 1 (
        echo Found broken duplicate code in src\boot\rom_head.c
        echo Moving src\boot to _broken_src_boot_backup
        if exist "%PROJECT%\_broken_src_boot_backup" rmdir /s /q "%PROJECT%\_broken_src_boot_backup"
        move "%PROJECT%\src\boot" "%PROJECT%\_broken_src_boot_backup" >nul
    )
)

echo Building ROM...
"%SGDK%\bin\make.exe" -f "%SGDK%\makefile.gen"
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b %errorlevel%
)

if not exist "%ROM%" (
    echo ROM not found: %ROM%
    pause
    exit /b 1
)

echo Starting BlastEm...
"%BLASTEM%" "%ROM%"
