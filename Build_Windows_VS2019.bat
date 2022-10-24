@echo off
cmake -S . -B build -G "Visual Studio 16 2019"
cmake --build build --config Debug