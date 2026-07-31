flushed=no

if systemctl is-active --quiet systemd-resolved; then
    resolvectl flush-caches
    echo 'systemd-resolved: cache flushed'
    flushed=yes
fi

# The others keep no interface of their own for this, so they are restarted instead.
for service in nscd dnsmasq unbound; do
    if systemctl is-active --quiet $service; then
        systemctl restart $service
        echo "$service: restarted"
        flushed=yes
    fi
done

if [ $flushed = no ]; then
    echo 'No name cache is running on this machine: nothing to flush'
fi
