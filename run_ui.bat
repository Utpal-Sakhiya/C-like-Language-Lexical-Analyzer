@echo off
REM Launch the web UI for the lexical analyzer (builds lexer.exe first if needed)
cd /d "%~dp0"
if not exist lexer.exe (
    where g++ >nul 2>nul
    if errorlevel 1 (
        if exist "C:\msys64\ucrt64\bin\g++.exe" set "PATH=C:\msys64\ucrt64\bin;%PATH%"
        if exist "C:\msys64\mingw64\bin\g++.exe" set "PATH=C:\msys64\mingw64\bin;%PATH%"
    )
    echo Building lexer.exe...
    g++ -std=c++17 -Wall -O2 -static -o lexer.exe src\main.cpp src\lexer.cpp src\report.cpp
    if errorlevel 1 (echo Build failed & pause & exit /b 1)
)
python ui\server.py %*
if errorlevel 1 pause
