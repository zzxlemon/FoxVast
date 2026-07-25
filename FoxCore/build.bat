@echo off
cd /d "%~dp0"

:: Force terminal to UTF-8 mode
chcp 936 > nul

if "%~1"=="clean" goto CLEAN

echo ====================================
echo BUILDING....
echo ====================================

if not exist "build" mkdir "build"
if not exist "builds" mkdir "builds"
del /q "builds\*.o" 2>nul

setlocal enabledelayedexpansion

:: Auto scan current directory and subdirectories for header file paths
set "include_paths="
for /r %%d in (.) do (
    if exist "%%d\*.h" set "include_paths=!include_paths! -I"%%d""
    if exist "%%d\*.hpp" set "include_paths=!include_paths! -I"%%d""
)

echo [1/2] Compiling source files...
set "obj_files="
set "has_cpp=0"

:: Loop to compile all .cpp files in current directory and subdirectories
for /r %%f in (*.cpp) do (
    set "has_cpp=1"
    echo -^> Compiling: %%~nxf ...
    
    :: Use UTF-8 for compilation
    g++ -c "%%f" %include_paths% -I ./native/glfw/include -I ./native/glad/include -o "builds\%%~nxf.o" -finput-charset=UTF-8 -fexec-charset=UTF-8
    
    if !errorlevel! neq 0 (
        echo -----------------------------------
        echo [ERROR] File %%~nxf compilation failed.
        goto END
    )
    set "obj_files=!obj_files! "builds\%%~nxf.o""
)

if "%has_cpp%"=="0" (
    echo [ERROR] No .cpp files found in current directory or subdirectories.
    goto END
)

echo -----------------------------------
echo [2/2] Linking and generating executable...
g++ %obj_files% -L ./native/glfw/lib -lws2_32 -lglfw3 -lopengl32 -lgdi32 -luser32 -lkernel32 -o build/fox.exe

if %errorlevel% equ 0 (
    echo -----------------------------------
    echo [SUCCESS] Compilation and linking successful!
) else (
    echo ------------------------------------
    echo [ERROR] Linking failed, please check error messages above.
)
goto END

:CLEAN
if exist "builds" rd /s /q "builds"
if exist "build" rd /s /q "build"
echo -----------------------------------
echo [SUCCESS] Temporary files cleaned.
goto END

:END
echo ---------------END-----------------
