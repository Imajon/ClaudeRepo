@echo off
rem Remet une console a zero : efface toute la memoire (progression, evolution, glitchs, pieces, classement), puis remet le jeu.
set CLI="C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
set ESPTOOL="%LOCALAPPDATA%\Arduino15\packages\esp32\tools\esptool_py\5.3.1\esptool.exe"
set FQBN=esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=huge_app
cd /d "%~dp0"
set ARDUINO_DIRECTORIES_USER=%~dp0libs
set PORT=
for /f "delims=" %%P in ('python find_esp32_port.py') do set PORT=%%P
if "%PORT%"=="" set /p PORT=Port de la console (ex. COM12) :
echo Effacement de toute la memoire de la console sur %PORT%...
%ESPTOOL% --chip esp32c3 -p %PORT% erase-flash || goto :err
echo Remise du jeu...
%CLI% compile --fqbn %FQBN% c3-tft-meatboy.ino && %CLI% upload -p %PORT% --fqbn %FQBN% c3-tft-meatboy.ino || goto :err
echo Console remise a zero.
pause
exit /b 0
:err
echo ECHEC.
pause
exit /b 1
