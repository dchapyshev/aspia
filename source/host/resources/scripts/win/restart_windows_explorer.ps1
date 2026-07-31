Get-Process explorer -ErrorAction SilentlyContinue |
    Where-Object { $_.SessionId -eq $SessionId } | Stop-Process -Force
Start-Sleep -Seconds 2
if (-not (Get-Process explorer -ErrorAction SilentlyContinue |
    Where-Object { $_.SessionId -eq $SessionId }))
{
    Start-Process explorer.exe
}
Write-Host 'Windows Explorer restarted'
