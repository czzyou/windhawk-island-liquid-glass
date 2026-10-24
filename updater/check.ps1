param([ValidateSet('Manual','Rollback')][string]$Mode='Manual')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms
try {
    $sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    $base='Windhawk-Island-'+$sid
    foreach ($kind in @('Auto','Manual','Rollback')) {
        if ((Get-ScheduledTask -TaskName ($base+'-'+$kind)).State -eq 'Running') {
            [void][Windows.Forms.MessageBox]::Show('A check/update is already running.','Island updater')
            exit
        }
    }
    Start-ScheduledTask -TaskName ($base+'-'+$Mode)
    $shell=New-Object -ComObject WScript.Shell
    [void]$shell.Popup('Checking in the background. The result will appear when finished.',3,'Island updater',64)
} catch { [void][Windows.Forms.MessageBox]::Show($_.Exception.Message,'Island updater') }
