@echo off
call "E:\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
set "G09ROOT=E:\source\repos\SLSF Reloaded NG"
set "G09OUT=%TEMP%\slsf-group09-audit"
if not exist "%G09OUT%" mkdir "%G09OUT%"
cl /nologo /std:c++latest /O2 /MD /EHsc /utf-8 /I "%G09ROOT%\include" "%~dp0counterprobe.cpp" /Fo"%G09OUT%\counterprobe.obj" /Fe"%G09OUT%\counterprobe.exe" /link "%G09ROOT%\build\windows\x64\releasedbg\SLSFCore.lib"
if errorlevel 1 exit /b %errorlevel%
"%G09OUT%\counterprobe.exe"
exit /b %errorlevel%
