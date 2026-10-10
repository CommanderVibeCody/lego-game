@echo off
rem Configure, build and (optionally) run the game. Usage: build.bat [run]
cd /d "%~dp0"
cmake -B build -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build build --config Release --parallel || exit /b 1
echo Built: build\bin\lego-game.exe
if /i "%1"=="run" build\bin\lego-game.exe
