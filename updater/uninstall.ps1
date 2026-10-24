[CmdletBinding()]
param()
. (Join-Path $PSScriptRoot 'core.ps1')
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
$principal=New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $ps=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    Start-Process -FilePath $ps -ArgumentList ('-NoProfile -File "'+$PSCommandPath+'"') -Verb RunAs -WindowStyle Hidden -Wait
    exit
}
$config=Read-JsonFile (Join-Path $PSScriptRoot 'config.json')
if ($identity.User.Value -cne $config.userSid) { throw 'Uninstall as the user who installed this updater.' }
$mutex=New-Object Threading.Mutex($false,'Global\Windhawk-Island-Updater')
$locked=$false
try {
    try { $locked=$mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $locked=$true }
    if (-not $locked) { throw 'An update is running; wait for it to finish before uninstalling.' }
    $hostEnv=Get-WindhawkEnvironment
    if(Test-Path -LiteralPath $hostEnv.Storage){Set-UpdateAvailable $hostEnv 0}
    $baseName='Windhawk-Island-'+$config.userSid
    foreach ($mode in @('Auto','Manual','Rollback')) { Unregister-ScheduledTask -TaskName ($baseName+'-'+$mode) -Confirm:$false -ErrorAction SilentlyContinue }
    $menu=Join-Path ([Environment]::GetFolderPath('Programs')) 'Windhawk Island'
    foreach ($name in @('Check for updates.lnk','Roll back previous version.lnk','Uninstall updater.lnk')) {
        $path=Join-Path $menu $name
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
    }
    # Keep the installed plugin, user data and backups; remove only this updater's known files.
    foreach ($file in @('core.ps1','update.ps1','check.ps1','uninstall.ps1','public-key.xml','config.json')) {
        $path=Join-Path $PSScriptRoot $file
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
    }
    Write-Host 'Updater removed. Plugin, settings and backups were retained.'
} finally { if ($locked) { $mutex.ReleaseMutex() }; $mutex.Dispose() }
