$since = (Get-Date).AddDays(-30)
$boot = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
$uptime = (Get-Date) - $boot
Write-Host ('Last boot: {0} ({1} days {2} hours ago)' -f $boot, $uptime.Days, $uptime.Hours)

# 41 - the machine rebooted without shutting down cleanly, 1001 - it rebooted from a stop
# error, 6008 - the previous shutdown was unexpected. One event of the three is enough to
# call a restart unexpected; all three of them together usually describe the same one.
$records = @(Get-WinEvent -ErrorAction SilentlyContinue -FilterHashtable @{
    LogName = 'System'; Id = 41, 1001, 6008; StartTime = $since })

Write-Host ''
if ($records.Count -eq 0)
{
    Write-Host ('No unexpected shutdowns since {0:yyyy-MM-dd}' -f $since)
}
else
{
    Write-Host ('{0} unexpected shutdown record(s):' -f $records.Count)
    foreach ($record in ($records | Select-Object -First 20))
    {
        $text = ($record.Message -split '\r?\n')[0]
        if ($text.Length -gt 120) { $text = $text.Substring(0, 120) + '...' }
        Write-Host ('  {0} id {1}: {2}' -f
                    $record.TimeCreated.ToString('yyyy-MM-dd HH:mm'), $record.Id, $text)
    }
}

# 1074 names the process and the person behind a planned shutdown, which is what tells an
# ordinary restart from one nobody asked for.
$planned = @(Get-WinEvent -ErrorAction SilentlyContinue -FilterHashtable @{
    LogName = 'System'; Id = 1074; StartTime = $since })

Write-Host ''
Write-Host 'Last planned shutdowns:'
if ($planned.Count -eq 0)
{
    Write-Host '  none'
}
else
{
    foreach ($record in ($planned | Select-Object -First 5))
    {
        $text = ($record.Message -split '\r?\n')[0]
        if ($text.Length -gt 120) { $text = $text.Substring(0, 120) + '...' }
        Write-Host ('  {0}: {1}' -f
                    $record.TimeCreated.ToString('yyyy-MM-dd HH:mm'), $text)
    }
}
