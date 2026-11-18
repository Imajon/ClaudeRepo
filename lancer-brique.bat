@echo off
rem Serveur Brique local : http://localhost:8080  (Etabli : onglet de l'interface)
rem Option "test" : lancer-brique.bat test  -> port 8081, donnees data-test.json
rem Option "ia" : lancer-brique.bat ia  -> l'IA Mistral, cle lue sur la 1re ligne de 2026\code.txt (jamais affichee)
cd /d "%~dp0brique"
if /i "%1"=="ia" for /f "usebackq delims=" %%k in ("%~dp0..\code.txt") do if not defined MISTRAL_API_KEY set "MISTRAL_API_KEY=%%k"
set PORT=8080
if /i "%1"=="test" (
  set PORT=8081
  set DATA=data-test.json
)
start "" http://localhost:%PORT%
node server.js
pause
