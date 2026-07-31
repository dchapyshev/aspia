if (-not (Get-Command Get-MpComputerStatus -ErrorAction SilentlyContinue))
{
    throw 'Microsoft Defender is not installed on this computer'
}

$status = Get-MpComputerStatus
Write-Host ('Running mode: ' + $status.AMRunningMode)
Write-Host ('Signatures: ' + $status.AntivirusSignatureVersion +
            ' of ' + $status.AntivirusSignatureLastUpdated)

Write-Host 'Updating signatures...'
Update-MpSignature
Write-Host ('Signatures: ' + (Get-MpComputerStatus).AntivirusSignatureVersion)

# Everything Defender knew about before the scan started is history of its own, so only what
# it finds from now on says anything about the state of this computer.
$started = Get-Date
Write-Host ''
Write-Host 'Running a quick scan, this takes a few minutes...'
Start-MpScan -ScanType QuickScan

$found = @(Get-MpThreatDetection -ErrorAction SilentlyContinue |
    Where-Object { $_.InitialDetectionTime -ge $started })
if ($found.Count -eq 0)
{
    Write-Host 'Quick scan finished, nothing found'
}
else
{
    Write-Host ('Quick scan finished, ' + $found.Count + ' detection(s):')
    $found | Format-Table InitialDetectionTime, ThreatID, Resources -AutoSize
}
