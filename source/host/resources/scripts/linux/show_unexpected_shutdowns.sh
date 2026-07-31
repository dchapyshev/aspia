echo 'Boots known to the journal:'
journalctl --list-boots --no-pager | tail -10
if [ "$(journalctl --list-boots --no-pager | wc -l)" -le 1 ]; then
    echo
    echo 'The journal of this machine is not kept between boots, so only the current one'
    echo 'is listed. The records below come from the login database instead.'
fi
echo

echo 'Reboots and shutdowns:'
last -x reboot shutdown 2>/dev/null | head -20 || echo '  the login database is empty'
echo
echo 'A reboot with no shutdown recorded before it is an unexpected one.'
