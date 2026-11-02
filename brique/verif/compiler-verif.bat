@echo off
rem Compile verif.exe (verification des jeux Berry) avec Visual Studio. Sur le serveur : "postinstall" de package.json.
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0"
if not exist obj mkdir obj
cl /nologo /O2 /I..\..\c3-berry\src\berry\src verif.c ..\..\c3-berry\src\berry\src\*.c /Fe:verif.exe /Fo:obj\
