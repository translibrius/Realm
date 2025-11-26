@echo off
set CTRL_C_EXIT=1
setlocal

echo === Configuring with CMake ===
cmake --preset debug
if %ERRORLEVEL% neq 0 goto fail

echo.
echo === Building project ===
cmake --build --preset debug
if %ERRORLEVEL% neq 0 goto fail

echo.
echo === Running executable ===
.\build\debug\bin\realm.exe && exit 0 || exit 1
if %ERRORLEVEL% neq 0 goto fail

echo.
echo === Finished successfully ===
pause
exit /b 0

:fail
echo.
echo *** ERROR: A step failed. ***
pause
exit /b 1
