$filter = "StartMode = 'Auto' AND State <> 'Running'"
$services = @(Get-CimInstance Win32_Service -Filter $filter)

if ($services.Count -eq 0)
{
    Write-Host 'Every service set to start with the system is running'
}
else
{
    Write-Host ('{0} service(s) set to start with the system are not running:' -f
                $services.Count)

    # Written out by hand rather than as a table: the names of some services are longer
    # than a console window is wide, and a table of them ends up unreadable.
    foreach ($service in ($services | Sort-Object DisplayName))
    {
        $name = $service.DisplayName
        if ($name.Length -gt 45) { $name = $name.Substring(0, 45) + '...' }
        if ($service.DelayedAutoStart) { $name = $name + ' (delayed)' }
        Write-Host ('  {0,-58} {1}' -f $name, $service.Name)
    }

    # A service that stops itself is not a broken one, and there are enough of them on a
    # healthy machine to make the list confusing without saying so.
    Write-Host 'Some of these stop on their own once their work is done (licensing,'
    Write-Host 'updaters), and a delayed one may still be starting after a boot.'
}
