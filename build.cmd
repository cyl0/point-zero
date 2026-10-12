@echo off
setlocal
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "PZ_VS=%%i"
if not defined PZ_VS exit /b 1
call "%PZ_VS%\VC\Auxiliary\Build\vcvars64.bat" 10.0.26100.0 -vcvars_ver=14.44.35207 >nul
if errorlevel 1 exit /b %errorlevel%
set "PZ_CMAKE=%PZ_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "PZ_NINJA=%PZ_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
"%PZ_CMAKE%" -S "%~dp0." -B "%~dp0build\release" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="%PZ_NINJA%"
if errorlevel 1 exit /b %errorlevel%
"%PZ_CMAKE%" --build "%~dp0build\release" --parallel 6 --target point_zero_loader point_zero_updater point_zero_core
exit /b %errorlevel%
