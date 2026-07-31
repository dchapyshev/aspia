Write-Host 'Creating a restore point, this can take a minute...'
$before = @(Get-ComputerRestorePoint).Count
Checkpoint-Computer -Description 'Aspia' -RestorePointType MODIFY_SETTINGS
if (@(Get-ComputerRestorePoint).Count -gt $before)
{
    Write-Host 'Restore point created'
}
else
{
    Write-Host 'No restore point was created: Windows keeps one per 24 hours'
}
