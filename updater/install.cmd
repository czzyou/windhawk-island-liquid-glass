@echo off
set "ISLAND_SETUP=%~dp0setup.ps1"
powershell.exe -NoProfile -Command "$ErrorActionPreference='Stop'; $dir = Split-Path -LiteralPath $env:ISLAND_SETUP; foreach ($name in @('setup.ps1','core.ps1','update.ps1','check.ps1','uninstall.ps1')) { Unblock-File -LiteralPath (Join-Path $dir $name) }; $p = Start-Process -FilePath ($env:SystemRoot + '\System32\WindowsPowerShell\v1.0\powershell.exe') -ArgumentList ('-NoProfile -File '+[char]34+$env:ISLAND_SETUP+[char]34) -Verb RunAs -Wait -PassThru; exit $p.ExitCode"
if errorlevel 1 pause
