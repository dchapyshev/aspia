before=$(df -k --output=avail / | tail -1)

echo '=== Journal'
journalctl --disk-usage
journalctl --vacuum-time=7d 2>&1 | tail -2

echo
echo '=== Package cache'
if command -v dnf > /dev/null; then
    dnf clean all
elif command -v apt-get > /dev/null; then
    apt-get clean
    echo 'apt: cache cleared'
else
    echo 'no known package manager'
fi

echo
echo '=== Temporary files'
systemd-tmpfiles --clean 2>&1 | tail -2
echo 'done'

# The paths of the user come from the service: this shell belongs to the system account and
# knows nothing about the person the machine is being cleaned for.
echo
echo "=== Cache and trash of $SESSION_USER"
du -sh "$SESSION_HOME/.cache" 2> /dev/null
rm -rf "$SESSION_HOME/.cache/"* 2> /dev/null
rm -rf "$SESSION_HOME/.local/share/Trash/"* 2> /dev/null
echo 'cleared'

after=$(df -k --output=avail / | tail -1)
echo
echo "Freed on /: $(( (after - before) / 1024 )) MB"
