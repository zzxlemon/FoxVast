@echo off
cmake -S . -B ..\cmake-build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1
cmake --build ..\cmake-build --target fox
if errorlevel 1 exit /b 1
cmake --build ..\cmake-build --target math random file util socket graphics time
