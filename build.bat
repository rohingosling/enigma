@echo off
rem -------------------------------------------------------------------------------------------------------------------
rem Project: Enigma
rem Version: 1.0
rem Date:    2026-10-16
rem Author:  Rohin Gosling
rem
rem Description:
rem
rem   Build driver for Enigma, run by cmd.exe from any directory (spec 9.1, architecture 10.1).
rem
rem   - build.bat          Builds build\enigma.exe and, on success, increments the incarnation counter.
rem   - build.bat test     Builds and runs build\enigma_test.exe, returning its exit code. The counter is unchanged.
rem   - build.bat clean    Deletes build\enigma.exe and build\enigma_test.exe. The counter is unchanged.
rem
rem   The counter lives in incarnation.txt. A release build reads N, compiles with -DENIGMA_INCARNATION=N+1, and writes
rem   N+1 back only if the build succeeds, so every incarnation number belongs to an executable that was built.
rem
rem TODO:
rem
rem   1. None.
rem
rem -------------------------------------------------------------------------------------------------------------------

setlocal

rem Work from the project root, whatever the caller's current directory (BLD-01).

cd /d "%~dp0"

rem Flags shared by the release and test builds, so the two cannot drift apart (spec 9.3, BLD-20, BLD-21).

set "COMPILER_FLAGS=-std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -static"

rem Every src\*.cpp except main.cpp, listed explicitly rather than by wildcard. Each phase adds its modules here.

set "ENIGMA_SOURCES=src\command_line.cpp src\perceptron.cpp src\random_source.cpp src\report.cpp src\statistics.cpp src\table.cpp"

rem The test runner, which the test build compiles in place of src\main.cpp (architecture 12).

set "TEST_MAIN_SOURCE=test\test_main.cpp"

set "COUNTER_FILE=incarnation.txt"
set "RELEASE_EXECUTABLE=build\enigma.exe"
set "TEST_EXECUTABLE=build\enigma_test.exe"

rem Windows tools are called by full path, because Git for Windows and MSYS2 put Unix tools of the same name on PATH.

set "SYSTEM_DIRECTORY=%SystemRoot%\System32"

rem Check for the compiler (BLD-04), and create the output directory, which a fresh clone can lack (BLD-02).

"%SYSTEM_DIRECTORY%\where.exe" g++ >nul 2>&1
if %ERRORLEVEL% neq 0 goto :compiler_missing
if not exist build mkdir build

rem Dispatch on the subcommand (BLD-03).

if not "%~2"=="" goto :usage_error
if "%~1"=="" goto :release
if /i "%~1"=="test" goto :test
if /i "%~1"=="clean" goto :clean
goto :usage_error

rem -------------------------------------------------------------------------------------------------------------------
rem Release build.
rem -------------------------------------------------------------------------------------------------------------------

:release

rem Read the counter, N. A missing file counts as 0 and is created when the build succeeds (BLD-12).

set "CURRENT_INCARNATION=0"
if not exist "%COUNTER_FILE%" goto :compile_release

rem Validate the counter before compiling (BLD-10, BLD-12): exactly one line, holding 0 or a number with no leading
rem zero, ending in CR LF, LF, or nothing. findstr inspects the file itself, so cmd.exe never expands unvalidated
rem text. The three searches below must each find nothing (exit code 1): a non-digit, a leading zero, an empty line.
rem findstr's /x switch and $ anchor are avoided because they match only before a CR, which an LF-only file lacks.
rem The digit classes are spelled out because the findstr range [0-9] also matches superscript digits. Rejecting
rem leading zeros stops set /a from reading the number as octal.

"%SYSTEM_DIRECTORY%\findstr.exe" /r "[^0123456789]" "%COUNTER_FILE%" >nul
if %ERRORLEVEL% neq 1 goto :counter_malformed
"%SYSTEM_DIRECTORY%\findstr.exe" /r "^0[0123456789]" "%COUNTER_FILE%" >nul
if %ERRORLEVEL% neq 1 goto :counter_malformed
"%SYSTEM_DIRECTORY%\findstr.exe" /r /v "." "%COUNTER_FILE%" >nul
if %ERRORLEVEL% neq 1 goto :counter_malformed

rem Every line is now a non-empty number, so for /f, which skips only empty lines, counts them all.

set "LINE_COUNT=0"
for /f "usebackq" %%L in ("%COUNTER_FILE%") do set /a LINE_COUNT+=1
if not "%LINE_COUNT%"=="1" goto :counter_malformed

set /p CURRENT_INCARNATION=<"%COUNTER_FILE%"

rem At most 9 digits, so the 32-bit signed arithmetic of set /a cannot overflow. The largest value cannot be
rem incremented without exceeding that limit.

if not "%CURRENT_INCARNATION:~9%"=="" goto :counter_malformed
if "%CURRENT_INCARNATION%"=="999999999" goto :counter_exhausted

:compile_release

set /a NEXT_INCARNATION=CURRENT_INCARNATION + 1

g++ %COMPILER_FLAGS% -s -DENIGMA_INCARNATION=%NEXT_INCARNATION% -o %RELEASE_EXECUTABLE% %ENIGMA_SOURCES% src\main.cpp
set "COMPILER_EXIT_CODE=%ERRORLEVEL%"
if %COMPILER_EXIT_CODE% neq 0 goto :compile_failed

rem Write N+1 back now that the build has succeeded (BLD-11). The parentheses stop a trailing digit from being parsed
rem as a stream redirection, as in "echo 1>file", which would write "ECHO is on." instead of the number.

(echo %NEXT_INCARNATION%)>"%COUNTER_FILE%" || goto :counter_write_failed

echo Built %RELEASE_EXECUTABLE%, incarnation %NEXT_INCARNATION%.
exit /b 0

rem -------------------------------------------------------------------------------------------------------------------
rem Test build.
rem -------------------------------------------------------------------------------------------------------------------

:test

rem The published tree may omit test\, so check for the test sources first (BLD-05). The counter is never touched
rem here (BLD-13).

if not exist "%TEST_MAIN_SOURCE%" goto :tests_missing

rem Compile with the release flags, minus -s and -DENIGMA_INCARNATION, plus -Isrc (BLD-21). Without the definition,
rem the test build reports incarnation 0 (BLD-14).

g++ %COMPILER_FLAGS% -Isrc -o %TEST_EXECUTABLE% %ENIGMA_SOURCES% %TEST_MAIN_SOURCE%
set "COMPILER_EXIT_CODE=%ERRORLEVEL%"
if %COMPILER_EXIT_CODE% neq 0 goto :test_compile_failed

rem Run the tests from the project root, so golden-file paths resolve against it, and return their exit code.

"%TEST_EXECUTABLE%"
exit /b %ERRORLEVEL%

rem -------------------------------------------------------------------------------------------------------------------
rem Clean.
rem -------------------------------------------------------------------------------------------------------------------

:clean

rem Delete both executables. The counter is never touched here (BLD-13). del can fail without setting ERRORLEVEL,
rem for example when an executable is still running, so check that the files are really gone.

if exist "%RELEASE_EXECUTABLE%" del /q "%RELEASE_EXECUTABLE%"
if exist "%TEST_EXECUTABLE%" del /q "%TEST_EXECUTABLE%"
if exist "%RELEASE_EXECUTABLE%" goto :clean_failed
if exist "%TEST_EXECUTABLE%" goto :clean_failed
echo Cleaned %RELEASE_EXECUTABLE% and %TEST_EXECUTABLE%.
exit /b 0

rem -------------------------------------------------------------------------------------------------------------------
rem Errors.
rem -------------------------------------------------------------------------------------------------------------------

:usage_error
>&2 echo Usage: build.bat [test ^| clean]
exit /b 1

:compiler_missing
>&2 echo build.bat: g++ was not found on PATH. Add the MSYS2 MINGW64 bin directory, for example C:\msys64\mingw64\bin.
exit /b 1

:counter_malformed
>&2 echo build.bat: %COUNTER_FILE% is malformed. It must hold one line: 0, or up to 9 digits with no leading zero.
>&2 echo build.bat: nothing was compiled.
exit /b 1

:counter_exhausted
>&2 echo build.bat: %COUNTER_FILE% holds 999999999, the largest incarnation the counter can hold.
>&2 echo build.bat: nothing was compiled.
exit /b 1

:compile_failed
>&2 echo build.bat: the build failed, so %COUNTER_FILE% is unchanged.
exit /b %COMPILER_EXIT_CODE%

:tests_missing
>&2 echo build.bat: %TEST_MAIN_SOURCE% was not found, so there are no tests to build. This tree omits test\.
exit /b 1

:test_compile_failed
>&2 echo build.bat: the test build failed.
exit /b %COMPILER_EXIT_CODE%

:clean_failed
>&2 echo build.bat: could not delete every executable. Close any running copy of Enigma and try again.
exit /b 1

:counter_write_failed

rem The executable carries incarnation N+1, but the counter still reads N, so the next build would reuse the number.
rem Delete the executable, so that the failed write leaves no build behind (BLD-11).

if exist "%RELEASE_EXECUTABLE%" del /q "%RELEASE_EXECUTABLE%"
>&2 echo build.bat: could not write %COUNTER_FILE%, so %RELEASE_EXECUTABLE% was deleted. Nothing was built.
exit /b 1
