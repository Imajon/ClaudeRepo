@echo off
rem Variante prototype : ecran SCL,SDA,RES,DC,CS sur GPIO0..4 (BLK->3V3), boutons GAUCHE=10 DROITE=21 SAUT=5.
set CLI="C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
set FQBN=esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=huge_app
set PORT=COM23
set AUTO=
cd /d "%~dp0"
set ARDUINO_DIRECTORIES_USER=%~dp0libs
for /f "delims=" %%P in ('python find_esp32_port.py') do (set PORT=%%P& set AUTO=1)
python gen_quiz.py || (echo ECHEC quiz & pause & exit /b 1)
python gen_hand_levels.py || (echo ECHEC niveaux & pause & exit /b 1)
python make_proto_ino.py || (echo ECHEC proto & pause & exit /b 1)
if "%1"=="upload" set AUTO=1
if defined AUTO (
  %CLI% compile --fqbn %FQBN% proto-0-4 && %CLI% upload -p %PORT% --fqbn %FQBN% proto-0-4
) else (
  %CLI% compile --fqbn %FQBN% proto-0-4
)
pause
