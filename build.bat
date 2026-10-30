@echo off
REM Build on Windows with MinGW g++
cd /d "%~dp0"
REM If g++ is not on PATH, fall back to the default MSYS2 install locations.
where g++ >nul 2>nul
if %errorlevel% neq 0 (
    if exist "C:\msys64\ucrt64\bin\g++.exe" (
        set "PATH=C:\msys64\ucrt64\bin;%PATH%"
    ) else if exist "C:\msys64\mingw64\bin\g++.exe" (
        set "PATH=C:\msys64\mingw64\bin;%PATH%"
    ) else (
        echo g++ not found. Install MinGW/MSYS2 or add its bin folder to PATH.
        pause & exit /b 1
    )
)
g++ -std=c++17 -Wall -O2 -static -o lexer.exe src\main.cpp src\lexer.cpp src\report.cpp
if %errorlevel% neq 0 (echo Build failed & pause & exit /b 1)
echo Build successful. Starting analyzer...
.\lexer.exe
