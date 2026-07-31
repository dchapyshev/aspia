$queue = Join-Path $env:SystemRoot 'System32\spool\PRINTERS'
Stop-Service Spooler -Force
$files = @(Get-ChildItem $queue -File -ErrorAction SilentlyContinue)
Write-Host ('Removing ' + $files.Count + ' file(s) from the print queue')
$files | Remove-Item -Force -ErrorAction SilentlyContinue
Start-Service Spooler
Get-Service Spooler | Format-Table Name, Status -AutoSize
