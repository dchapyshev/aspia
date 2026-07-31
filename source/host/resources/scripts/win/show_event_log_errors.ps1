$since = (Get-Date).AddHours(-24)
$total = 0

foreach ($log in 'System', 'Application')
{
    # Level 1 is critical and level 2 is an error: what the system itself calls a failure.
    # An empty result is reported as an error by the cmdlet, hence the silent action.
    $records = @(Get-WinEvent -ErrorAction SilentlyContinue -FilterHashtable @{
        LogName = $log; Level = 1, 2; StartTime = $since })
    $total += $records.Count

    Write-Host ''
    Write-Host ('{0}: {1} record(s)' -f $log, $records.Count)

    # Grouped by the source: twenty records of one failing service are one problem, not
    # twenty, and the source with the most records is the one worth reading first.
    foreach ($group in ($records | Group-Object ProviderName | Sort-Object Count -Descending))
    {
        Write-Host ''
        Write-Host ('  {0} ({1})' -f $group.Name, $group.Count)

        foreach ($record in ($group.Group | Select-Object -First 3))
        {
            $text = ($record.Message -split '\r?\n')[0]
            if ($text.Length -gt 100) { $text = $text.Substring(0, 100) + '...' }
            Write-Host ('    {0} id {1}: {2}' -f
                        $record.TimeCreated.ToString('MM-dd HH:mm'), $record.Id, $text)
        }
    }
}

Write-Host ''
Write-Host ('Total: {0} record(s) since {1}' -f $total, $since)
