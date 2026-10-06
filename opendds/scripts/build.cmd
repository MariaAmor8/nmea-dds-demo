@echo off
setlocal
pushd "%~dp0..\.."
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File opendds\scripts\build.ps1 %*
set "marine_exit=%errorlevel%"
popd
exit /b %marine_exit%
