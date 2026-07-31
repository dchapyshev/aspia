$total = 0
$paths = @((Join-Path $UserProfile 'AppData\Local\Temp'),
           (Join-Path $env:SystemRoot 'Temp'))

# The recycle bin of a user is a folder named after their identifier on every drive. The
# identifier is the one the system keeps the profile of this user under.
$profiles = 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList'
$sid = (Get-ChildItem $profiles | Where-Object {
    (Get-ItemProperty $_.PSPath).ProfileImagePath -eq $UserProfile }).PSChildName
if ($sid)
{
    foreach ($disk in Get-CimInstance Win32_LogicalDisk -Filter 'DriveType = 3')
    {
        $paths += (Join-Path $disk.DeviceID ('$Recycle.Bin\' + $sid))
    }
}

foreach ($path in $paths)
{
    if (-not (Test-Path $path)) { continue }
    $before = (Get-ChildItem $path -Force -Recurse -File -ErrorAction SilentlyContinue |
        Measure-Object -Property Length -Sum).Sum
    Get-ChildItem $path -Force -ErrorAction SilentlyContinue |
        Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    $after = (Get-ChildItem $path -Force -Recurse -File -ErrorAction SilentlyContinue |
        Measure-Object -Property Length -Sum).Sum
    Write-Host ($path + ': freed ' + [math]::Round(($before - $after) / 1MB, 1) + ' MB')
    $total += $before - $after
}
Write-Host ('Total freed: ' + [math]::Round($total / 1MB, 1) + ' MB')
