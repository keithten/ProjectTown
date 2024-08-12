@echo off
REM --- Remove built libs and header from unreal plugin directory ---

REM --- COMMAND ARGUMENTS ---
if '%1'=='' (
	echo Platform expected as first parameter
	exit /b 1
)
if '%2'=='' (
	echo Configuration expected as second parameter
	exit /b 1
)
set PLT=%1
set CFG=%2

REM --- PATHS ---
set THDPTY=Source\ThirdParty
set LIB_1=%THDPTY%\Apparance\lib
set LIB_2=%THDPTY%\Apparance\lib\%PLT%
set LIB=%THDPTY%\Apparance\lib\%PLT%\%CFG%
set INC=%THDPTY%\Apparance\inc

REM --- DEL ---
if not exist %LIB%\ApparanceCoreLib.lib goto SKIP1
del /F /Q %LIB%\ApparanceCoreLib.lib >nul
if ERRORLEVEL 1 (
	echo Failed to delete Apparance Core lib
	exit /b 2
) else (
	echo Deleted %LIB%\ApparanceCoreLib.lib
)
:SKIP1

if not exist %LIB%\ApparanceEngineLib.lib goto SKIP2
del /F /Q %LIB%\ApparanceEngineLib.lib >nul
if ERRORLEVEL 1 (
	echo Failed to delete Apparance Engine lib
	exit /b 2
) else (
	echo Deleted %LIB%\ApparanceEngineLib.lib
)
:SKIP2
:SKIP_LIB

if not exist %INC%\Apparance.h goto SKIP3
del /F /Q %INC%\Apparance.h >nul
if ERRORLEVEL 1 (
	echo Failed to delete Apparance header
	exit /b 2
) else (
	echo Deleted %INC%\Apparance.h
)
:SKIP3
:SKIP_INC
	