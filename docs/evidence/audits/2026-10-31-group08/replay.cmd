@echo off
setlocal
call E:\BuildTools\VC\Auxiliary\Build\vcvars64.bat >nul
if errorlevel 1 exit /b %errorlevel%
for %%I in ("%~dp0..\..\..\..") do set "SLSF_G08_REPO=%%~fI"
set "SLSF_G08_OUT=%TEMP%\slsf-group08-audit-replay"
if not exist "%SLSF_G08_OUT%" mkdir "%SLSF_G08_OUT%"
cl /nologo /std:c++latest /O2 /MD /EHsc /utf-8 /I"%SLSF_G08_REPO%\include" /Fo"%SLSF_G08_OUT%\probe.obj" /Fe"%SLSF_G08_OUT%\probe.exe" "%~dp0probe.cpp" "%SLSF_G08_REPO%\build\windows\x64\releasedbg\SLSFCore.lib"
if errorlevel 1 exit /b %errorlevel%
"%SLSF_G08_OUT%\probe.exe"
exit /b %errorlevel%
