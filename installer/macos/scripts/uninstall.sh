#!/bin/sh
#
# Removes Aspia Host after its bundle is gone from /Applications, which is what dragging it to the
# Trash does. launchd starts it as root whenever the bundle path changes. The settings of the host and
# the privacy permissions given to it stay, so a host installed again comes back as it was.
#

APP="/Applications/Aspia Host.app"
LABEL="org.aspia.host-uninstall"
DAEMON_LABEL="org.aspia.host-service"
AGENT_LABEL="org.aspia.host-loginwindow"

[ -e "$APP" ] && exit 0

launchctl bootout "system/$DAEMON_LABEL" 2>/dev/null || true
rm -f "/Library/LaunchDaemons/$DAEMON_LABEL.plist"

# The agent runs in every GUI session a user is logged in to.
for uid in $(ps -axo uid=,ucomm= | awk '$2 == "aspia_host" { print $1 }' | sort -u); do
    launchctl bootout "gui/$uid/$AGENT_LABEL" 2>/dev/null || true
done
rm -f "/Library/LaunchAgents/$AGENT_LABEL.plist"

pkill -x aspia_host 2>/dev/null || true

seconds=0
while pgrep -x aspia_host >/dev/null 2>&1 && [ "$seconds" -lt 5 ]; do
    sleep 1
    seconds=$((seconds + 1))
done

pkill -KILL -x aspia_host 2>/dev/null || true

# The PAM policy of the terminal stays. /etc/pam.d is open to the installer only, and the policy does
# nothing without the host.
pkgutil --forget org.aspia.host >/dev/null 2>&1 || true

# The job ends here, so it goes last.
rm -f "/Library/LaunchDaemons/$LABEL.plist" "$0"
launchctl bootout "system/$LABEL" 2>/dev/null || true

exit 0
