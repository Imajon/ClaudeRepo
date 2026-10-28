@echo off
setlocal
set CLI="C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
set FQBN=esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=huge_app
set DEFAULT_PORT=COM23
cd /d "%~dp0"
rem bibliotheques du projet seulement (libs\libraries = jonctions vers les 3 libs Adafruit) : evite l'avertissement "Multiple libraries were found for WiFi.h"
set ARDUINO_DIRECTORIES_USER=%~dp0libs


set AUTO=
for /f "delims=" %%P in ('python find_esp32_port.py') do (set DEFAULT_PORT=%%P& set AUTO=1)
echo.
set PORT=%DEFAULT_PORT%
if not defined AUTO set /p PORT="Port COM a utiliser [%DEFAULT_PORT%] : "

echo Regeneration du quiz depuis questions.txt...
python gen_quiz.py
if errorlevel 1 (
  echo ECHEC generation quiz_data.h - compilation annulee.
  pause
  exit /b 1
)

echo Regeneration des niveaux dessines depuis hand_levels.txt...
python gen_hand_levels.py
if errorlevel 1 (
  echo ECHEC generation hand_levels_data.h - compilation annulee.
  pause
  exit /b 1
)

%CLI% compile --fqbn %FQBN% c3-tft-meatboy.ino && %CLI% upload -p %PORT% --fqbn %FQBN% c3-tft-meatboy.ino
pause
