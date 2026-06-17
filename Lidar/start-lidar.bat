@echo off
echo Synchronisation des sources...
xcopy /E /Y /I "C:\Users\info\Dropbox\Dossier_AntiFaktory\2026\ClaudeRepo\Lidar\src" "C:\LidarApp\src" >nul

echo Lancement de l'application...
cd /d "C:\LidarApp"
cmd /k "node_modules\electron\dist\electron.exe ."
