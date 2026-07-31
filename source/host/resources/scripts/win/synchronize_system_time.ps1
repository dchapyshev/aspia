Start-Service W32Time -ErrorAction SilentlyContinue
w32tm /resync /rediscover
w32tm /query /status
Write-Host ('Current time: ' + (Get-Date))
