# Installation script for localbit.
# https://github.com/Hixie/localbit
_() {
    set -eu
    if [[ ! -f usr/local/lb/etc/lb_scripts.conf ]]; then
        echo "This script should be run from the root of the cloudbit SD card filesystem."
        exit 1
    fi
    if ! grep --fixed-strings --line-regexp --quiet 'FIRMWAREVERSION=1.0.150603a' usr/local/lb/etc/lb_scripts.conf; then
        echo "This SD card appears to be for an incompatible cloudbit firmware version."
        echo "This script only knows about version 1.0.150603a."
        echo "See: https://littlebits.com/cloudbit-firmware"
        exit 1
    fi
    if [[ -d root/localbit ]]; then
        echo "It appears that this script has already been run on this SD card."
        exit 1
    fi
    if [[ -z "$1" ]]; then
        echo "Specify the host to which messages should be sent as the argument to this script."
        exit 1
    fi
    if [[ "$1" == "server.example.com" ]]; then
        echo "Replace 'server.example.com' with the name of the host to which messages should be sent."
        exit 1
    fi
    if [[ ! -w root ]]; then
        if (( EUID == 0 )); then
            echo "The cloudbit SD card filesystem is not writable. Was it mounted read-only?"
        else
            echo "The cloudbit SD card filesystem is not writable. You probably need to run as root, using sudo."
        fi
        exit 1
    fi
    echo "$1" > root/localbit_server.cfg
    git clone --depth 1 --quiet https://github.com/Hixie/localbit.git root/localbit
    mkdir etc/systemd/system/multi-user.target.wants/disabled
    mv etc/systemd/system/multi-user.target.wants/iptables.service etc/systemd/system/multi-user.target.wants/disabled
    mv etc/systemd/system/multi-user.target.wants/ADC.service etc/systemd/system/multi-user.target.wants/disabled
    mv etc/systemd/system/multi-user.target.wants/LEDcolor.service etc/systemd/system/multi-user.target.wants/disabled
    mv etc/systemd/system/multi-user.target.wants/button.service etc/systemd/system/multi-user.target.wants/disabled
    mv etc/systemd/system/multi-user.target.wants/dac_init.service etc/systemd/system/multi-user.target.wants/disabled
    mv etc/systemd/system/multi-user.target.wants/dac.service etc/systemd/system/multi-user.target.wants/disabled
    mv etc/systemd/system/multi-user.target.wants/netctl-monitor.service etc/systemd/system/multi-user.target.wants/disabled
    mv etc/systemd/system/multi-user.target.wants/onboot.service etc/systemd/system/multi-user.target.wants/disabled
    ln -s /root/localbit/localbit-startup.service etc/systemd/system/multi-user.target.wants/localbit-startup.service
    ln -s /root/localbit/localbit-daemon.service etc/systemd/system/multi-user.target.wants/localbit-daemon.service
    echo "Installation complete."
}
_ "$@"
