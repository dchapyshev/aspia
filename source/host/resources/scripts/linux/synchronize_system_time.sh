if systemctl is-active --quiet chronyd; then
    chronyc makestep
    chronyc tracking
elif systemctl is-active --quiet systemd-timesyncd; then
    timedatectl set-ntp true
    systemctl restart systemd-timesyncd
    sleep 2
    timedatectl timesync-status
else
    echo 'Neither chrony nor systemd-timesyncd is running on this machine'
fi

echo
timedatectl
