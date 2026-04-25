$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & "$vsWhere" -latest -property installationPath 2>$null
$devCmd = Join-Path $vsPath "Common7\Tools\VsDevCmd.bat"

# Create a temp batch that runs VsDevCmd then cmake
$tmpBat = Join-Path $PSScriptRoot "tmp_cmake.bat"
$buildDir = Join-Path $PSScriptRoot "build-clangd"

@"
@echo off
call "$devCmd" -arch=x64 >nul 2>&1
cd /d "$PSScriptRoot"
if exist "$buildDir" rmdir /s /q "$buildDir"
cmake -B "$buildDir" -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Release
if exist "$buildDir\compile_commands.json" (
    copy /Y "$buildDir\compile_commands.json" "$PSScriptRoot\compile_commands.json" >nul
    echo SUCCESS
) else (
    echo FAILED
)
if exist "$buildDir" rmdir /s /q "$buildDir"
"@ | Set-Content -Path $tmpBat -Encoding ASCII

# Run the batch
$output = & cmd.exe /c $tmpBat 2>&1
$output
Remove-Item $tmpBat -Force -ErrorAction SilentlyContinue
