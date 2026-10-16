@echo off
setlocal
rem Formations - build. Same arrangement as MapProbe: no engine .lib is linked,
rem every symbol is resolved by name at runtime.
set "VC=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VC%" goto :novc
call "%VC%" >nul
goto :build
:novc
echo Could not find vcvars64.bat.
exit /b 1
:build
if not exist bin mkdir bin
cl /nologo /LD /O2 /MT /EHa /std:c++17 /D_CRT_SECURE_NO_WARNINGS Formations.cpp /Fe:bin\Formations.dll /Fo:bin\ /Fd:bin\ /link /DLL /OUT:bin\Formations.dll kernel32.lib user32.lib
if errorlevel 1 goto :failed
rem the shapes, as a picture: formtest.exe [N] [dragLength]
cl /nologo /O2 /MT /EHsc /std:c++17 /D_CRT_SECURE_NO_WARNINGS formtest.cpp /Fe:bin\formtest.exe /Fo:bin\ft_ /Fd:bin\ft_ /link kernel32.lib
if errorlevel 1 goto :failed
echo built bin\Formations.dll and bin\formtest.exe
dumpbin /exports bin\Formations.dll | findstr /i startPlugin
exit /b 0
:failed
echo BUILD FAILED
exit /b 1
