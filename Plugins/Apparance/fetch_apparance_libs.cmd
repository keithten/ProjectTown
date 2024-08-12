@echo off
REM --- Pull built libs and header from apparance engine into unreal plugin directory so it can link against it ---

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
set SRC=..\Intermediate\%PLT%\%CFG%\Lib
set THDPTY=Source\ThirdParty
set LIB_1=%THDPTY%\Apparance\lib
set LIB_2=%THDPTY%\Apparance\lib\%PLT%
set LIB=%THDPTY%\Apparance\lib\%PLT%\%CFG%
set INC=%THDPTY%\Apparance\inc
set SRC_LIBPNG="..\3rdParty\LibPng\projects\vstudio\x64\Release Library"

REM --- ENSURE DIRECTORY STRUCTURE ---
if NOT exist %THDPTY% (
	mkdir %THDPTY%
)
if NOT exist %LIB_1% (
	mkdir %LIB_1%
)
if NOT exist %LIB_2% (
	mkdir %LIB_2%
)
if NOT exist %LIB% (
	mkdir %LIB%
)
if NOT exist %INC% (
	mkdir %INC%
)

REM --- COPY ---
copy /Y %SRC%\ApparanceCoreLib.lib %LIB%\ApparanceCoreLib.lib >nul
if ERRORLEVEL 1 (
	echo Failed to copy Apparance Core lib
	exit /b 2
) else (
	echo Copied %LIB%\ApparanceCoreLib.lib
)

copy /Y %SRC%\ApparanceEngineLib.lib %LIB%\ApparanceEngineLib.lib >nul
if ERRORLEVEL 1 (
	echo Failed to copy Apparance Engine lib
	exit /b 2
) else (
	echo Copied %LIB%\ApparanceEngineLib.lib
)

if '%CFG%'=='Shipping' goto no_pdb
copy /Y %SRC%\ApparanceEngineLib.pdb %LIB%\ApparanceEngineLib.pdb >nul
if ERRORLEVEL 1 (
	echo Failed to copy Apparance Engine PDB
	exit /b 2
) else (
	echo Copied %LIB%\ApparanceEngineLib.pdb
)
:no_pdb

copy /Y ..\ApparanceEngine\Apparance.h %INC%\Apparance.h >nul
if ERRORLEVEL 1 (
	echo Failed to copy Apparance header
	exit /b 2
) else (
	echo Copied %INC%\Apparance.h
)

REM - cimg dependencies -
copy /Y %SRC_LIBPNG%\libpng16.lib %LIB%\libpng16.lib >nul
if ERRORLEVEL 1 (
	echo Failed to copy libpng16 library
	exit /b 2
) else (
	echo Copied %LIB%\libpng16.lib
)
copy /Y %SRC_LIBPNG%\zlib.lib %LIB%\zlib.lib >nul
if ERRORLEVEL 1 (
	echo Failed to copy zlib library
	exit /b 2
) else (
	echo Copied %LIB%\zlib.lib
)

	