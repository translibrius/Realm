@echo off

cmake --build --preset debug
if errorlevel 1 goto build_failed

cd build\debug\bin || goto build_failed
Realm.exe
pause
exit /b

:build_failed
echo Build failed. Not running executable.
pause
exit /b 1
