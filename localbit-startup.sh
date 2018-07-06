#!/bin/bash
(
echo localbit-startup: init
source /etc/lb_scripts.conf
echo localbit-startup: Cloudbit firmware version $FIRMWAREVERSION

echo
ps aux
echo

echo localbit-startup: starting network...
netctl start cloudbit
echo localbit-startup: waiting for connection...
echo
while (! ping -c 1 $PING_DEST) do sleep 1s; done
echo

echo localbit-startup: setting date and time from network...
ntpdate -s $NTP_SERVER
systemctl start ntpd

echo localbit-startup: updating MAC address configuration file...
echo -n 'localbit-startup: MAC address is: '
sed s/://g /sys/class/net/$WADAP/address | tee $MACADDR | tee /root/mac_address.cfg
echo

echo localbit-startup: initializing hardware...
$DACPATH/DAC_init
$ADCPATH/ADC.d
sleep 2s
killall ADC.d

echo localbit-startup: reducing CPU clock speed...
$BUTILPATH/lowPowerSet.sh

echo
ps aux
echo

echo localbit-startup: done
) 2>&1 | tee /var/log/localbit-startup
