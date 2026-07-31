systemctl --failed --no-pager

# The list itself says nothing about the reason; it is in the log of the unit.
for unit in $(systemctl --failed --no-legend --plain | awk '{ print $1 }'); do
    echo
    echo "=== $unit"
    journalctl -u "$unit" -n 10 --no-pager -q
done
