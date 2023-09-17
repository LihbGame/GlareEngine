@echo off
# Configure the build
cmake -S . -B build

# Build debug binaries
cmake --build build --config Debug