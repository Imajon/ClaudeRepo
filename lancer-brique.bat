@echo off
rem Serveur Brique local : http://localhost:8080  (Etabli : onglet de l'interface)
rem Option "test" : lancer-brique.bat test  -> port 8081, donnees data-test.json
cd /d "%~dp0brique"
set PORT=8080
if /i "%1"=="test" (
  set PORT=8081
  set DATA=data-test.json
)
start "" http://localhost:%PORT%
node server.js
pause
