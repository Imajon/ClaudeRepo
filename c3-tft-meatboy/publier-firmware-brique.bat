@echo off
rem compile le firmware et le depose dans ..\brique\firmware\ (mise a jour des consoles depuis la page Brique)
cd /d "%~dp0"
python publier_firmware_brique.py
pause
