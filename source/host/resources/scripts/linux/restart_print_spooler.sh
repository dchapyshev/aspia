if ! systemctl cat cups.service > /dev/null 2>&1; then
    echo 'The printing service is not installed on this machine'
else
    cancel -a 2> /dev/null
    echo 'Print queue cleared'
    systemctl restart cups
    echo "Printing service: $(systemctl is-active cups)"
    lpstat -o 2> /dev/null
fi
