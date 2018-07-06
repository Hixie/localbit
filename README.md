# Local Network Cloudbit

## Introduction

The [Littlebits w20 cloudbit
bit](https://shop.littlebits.com/products/cloudbit) is a great idea:
it allows your Littlebits circuits to be connected to the Internet,
thus letting you integrate your contraptions with other services such
as Google Home (via IFTTT) or even your own homegrown home automation
systems. Unfortunately, the [Littlebits
Cloud](http://control.littlebitscloud.cc/) is very unreliable. For
something like this to work, you really need near-100% uptime, and in
my experience the API has an uptime of about 70%, sometimes being down
for weeks at a time.

In any case, it's a little crazy that a little circuit on your network
should communicate with a remote server if all it wants to do is tell
your Raspberry Pi on the other side of the room what the temperature
is or when a button gets pushed or when your laundry washing machine
has finished.

The cloudbit is just an embedded Linux computer, though, and
Littlebits kindly left a C compiler and a Perl interpreter on it, so
experimenting with the cloudbit is pretty straight-forward and solving
the problem of having to talk to the Internet just to send a message
six meters across the room is emminently addressable.


## Communicating with the local network instead of the cloud

This repository contains a replacement for the default behaviour of
the cloudbit, so that instead of reporting the current value of the
cloudbit to the Littlebits servers, it just sends it to a specified
local server.

There are three ways to install this program. In all three cases, you
will need a machine where you can mount ext2 file systems (e.g. a
Linux box, or a Mac using [FUSE for
macOS](https://osxfuse.github.io/)). Then, you either:

 * Use a bash shell to a script from this repository to complete the
   installation automatically.

 * Walk through the script's steps manually.

 * Enable SSH access on the cloudbit, then SSH into the cloudbit and
   install the software directly there using the script from the
   repository.


## Commissioning the cloudbit

Before you do anything described below, first make sure your cloudbit
is configured for your local network. The steps below disable the
Littlebits code that allows you to configure the network by pushing
the button on the cloudbit. To reconfigure the cloudbit after setting
things up as below, you will either need to reinstall the firmware
using the Littlebits updater, or manually figure it out.

You may also want to take note of the cloudbit's MAC address, since
that is used to identify the cloudbit.


## Mounting the SD card

The details for mounting the SD card vary from system to system.
Consult your local documentation (e.g. the `mount` program on Unix
systems) for instructions on doing this.

The SD card seems to have three partitions; you only need the third
one (the biggest one) which is where the Linux userspace filesystem is
stored.


## Installing automatically

_You will need: bash, sudo, curl, grep, git, mkdir, mv, ln_

_This has only been tested on Linux. It may work on other Unix-like
systems such as macOS, but this is untested._

Once you have the SD card mounted, run the command below.

This command prompts for your root password (because of the `sudo`).
This is necessary because the filesystem you have mounted has files
that are only writable by `root`. If you have configured your SD card
to be mounted so that it is user-writable on your host but will create
files that have `root` as the owner when run by the cloudbit, then you
can omit the `sudo`.

It's generally considered very sketchy to pipe a program from the
network into `sudo bash`; even piping it into bash without the `sudo`
isn't great since it can compromise your account. The shell script
downloads a binary onto your cloudbit; that binary could also be
compromised. I highly recommend going through the more tedious manual
process below instead, that way you have much more control over what
exactly is going on.

But if you're ok with trusting me and GitHub and are sure you're not
experiencing a state-level attack on your communications or some other
such thing, then run the following command, replacing
`server.example.com` with the name of the host to which you want to
send the network messages, in a shell open to the root directory of
the mounted SD card filesystem:

```
curl https://raw.githubusercontent.com/Hixie/localbit/master/install.sh | sudo bash -s server.example.com
```

Then, unmount the SD card, install it back in your cloudbit, and boot
up your cloudbit. If everything worked, the LED should turn off once
it has booted, and network traffic will start flowing to port 2020 of
the host you specified on the command line above. If the LED turns any
colour other than white, then something went wrong (unless you are
already sending your own packets to the cloudbit!).

### Security considerations

The script above disables the routing tables that keep the cloudbit
safe from being attacked (it has to, since the whole point is to
listen to UDP connections). It's possible that this will expose you to
some vulnerability in some software running on the cloudbit.


## Installing manually

A technically safer alternative to the above is to walk through the
following steps. You can either do it over SSH on the device itself,
or on the SD card directly.

The file `/usr/local/lb/etc/lb_scripts.conf` on the SD card ends with
a line that identifies the firmware version you are using. This
repository assumes you are using version 1.0.150603a, which as of the
time of writing is the latest version. If necessary, you can download
updates from
[littlebits.com](https://littlebits.com/cloudbit-firmware).


### Setting up SSH access for a w20 cloudbit

_If you are going to set everything up on the SD card directly, you
can skip this step._

Prior art:
[Chris Wilson](https://github.com/yepher/littlebits/blob/master/CloubitFileSystem.md),
[chobie](https://qiita.com/chobie@github/items/d41cfa2d60df5d7d1a3f),
[rhomel](https://gist.github.com/rhomel/0598aa7c63e61001e63f).

To enable SSH, you need to make three changes: changing the root
password, enabling the SSH service, and turning off iptables.

There's one user account set up, the root account. To change its
password, use `mkpasswd -m sha-512` to generate a new hash, and then
edit `/etc/shadow` and replace the hash (between the first two colons)
for the `root` user with the new hash. The password you give
`mkpasswd` will be the password you use to log in to your cloudbit.

To enable SSH, first edit `/etc/ssh/sshd_config`. There are two lines
to change. First, uncomment the line that says `PermitRootLogin yes`,
otherwise it will default to "no", which, as you might guess, disables
login. The other line to change is the one that says `UsePAM yes`;
change that to `UsePAM no`. The Pluggable Authentication Module (PAM)
interface overrides the root login setting.

Next, enable it by setting up symlinks in the
`/etc/systemd/system/multi-user.target.wants/` directory as follows:

```
sudo ln -s /usr/lib/systemd/system/sshdgenkeys.service sshdgenkeys.service
sudo ln -s /usr/lib/systemd/system/sshd.service sshd.service
```

Finally, you have to turn off iptables. Create a `disabled`
subdirectory in the `/etc/systemd/system/multi-user.target.wants/`
directory, and then move the `iptables.service` file in
`/etc/systemd/system/multi-user.target.wants/` into this new
`disabled` directory.

Once you are SSH'ed into the cloudbit, you can either continue
following these steps, or you can run the `install.sh` script
mentioned above (using the same instructions as above) to have the
script do them for you. This is slightly safer than running the script
on your machine, since if your cloudbit gets compromised, you only
lose your cloudbit, instead of your workstation.


### Configuring the local control client on the cloudbit device

You must specify the host name of the server to which messages will be
sent in `/root/localbit_server.cfg`. This file should contain only the
host name optionally followed by a newline character, and nothing
else.

From the root of the SD card filesystem (either after SSHing into the
cloudbit, or on your host with the SD card mounted), run the
following, replacing "server.example.com" with the host name that you
will be running your server on:

```
echo server.example.com > root/localbit_server.cfg
```


### Fetching the code

The code for the localbit software is on GitHub.

From the root of the SD card filesystem (either after SSHing into the
cloudbit, or on your host with the SD card mounted), run the following
to download it to your SD card:

```
git clone https://github.com/Hixie/localbit.git root/localbit
```

If you are running this on your host and have mounted the SD card
normally (i.e. honouring the permissions on the SD card), you may need
to run this with `sudo` since the SD card filesystem's `/root/`
directory is only writable by root.

The code for the program that sits between the cloudbit hardware and
the network is in
[`localbit.c`](https://github.com/Hixie/localbit/blob/master/localbit.c).


### Compiling the code

A compiled binary of localbit.c suitable for use on machines with the
architecture of cloudbits is also in this repository. However, if you
would prefer to compile it yourself, you may do so by SSHing into the
cloudbit and using the conveniently preinstalled `gcc` that Littlebits
provides.

Run this command in the `/root/localbit` directory (the one you
downloaded from git above) to regenerate the binary. You must be SSHed
into the cloudbit for this to work. In theory you could also
cross-compile from your host to the architecture of the cloudbit, but
that is left as an exercise for the reader.

```
gcc localbit.c -lm -o localbit
```

If you are compiling your own version, consider using `-O3` or
`-Ofast`. The version provided in the repository is currently compiled
with `-Ofast`, mostly to conserve power so that the bit doesn't heat
up too much.


### Turning off the built-in programs

If you followed the instructions earlier for enabling SSH, you will
have created a `disabled` subdirectory in the
`/etc/systemd/system/multi-user.target.wants/` directory on the
cloudbit SD card. If you did not do that, then do it now.

Then, move the following files from
`/etc/systemd/system/multi-user.target.wants/` into this `disabled`
directory:

* `iptables.service` (you will already have moved this if you enabled SSH)
* `ADC.service`
* `LEDcolor.service`
* `button.service`
* `dac_init.service`
* `dac.service`
* `netctl-monitor.service`
* `onboot.service`

Be aware that this will disable a number of features of the cloudbit:

Removing `button.service` disables the commissioning script (holding
down the button won't do anything anymore; normally `button.service`
runs a program that watches the button and triggers the commissioning
script if the button is help for long enough). If you ever need to
update the wifi configuration of this device, move these files back
into `/etc/systemd/system/multi-user.target.wants/` first.

Removing `netctl-monitor.service` will prevent `monitorNetctl.pl` from
running. That script is mainly in charge of managing the program that
sends the status of the cloudbit to the cloud, but, among other
things, it also runs the command that enables the network and sets the
system clock (there's no clock battery on the cloudbit).

Removing `dac_init.service` will prevent `DAC_init` from running at
startup, and that program must be run before any code uses the DAC.

The other services all implement various features mostly used by the
three called out explicitly above.

As a replacement for all the above, this repository comes with a short
shell script that enables the network, sets the date and time, and
calls the `DAC_init` program. To install it, run the following command
inside the `/etc/systemd/system/multi-user.target.wants/` directory:

```
sudo ln -s /root/localbit/localbit-startup.service localbit-startup.service
```

This script will log to `/var/log/localbit-startup` if you are curious
to see how successful it is.

This script does require internet access. It tries to ping `8.8.8.8`
(a Google DNS server) to test if the network is operational, and uses
NTP time servers to update the clock. (It honours the
`/usr/local/lb/etc/lb_scripts.conf` script, in particular to determine
which NTP server to use (`NTP_SERVER`), which IP address to ping to
test the network (`PING_DEST`), and the directories to use to find the
Littlebits binaries.)


### Turning on the localbit program

To automatically run the localbit binary and keep it running, run the
following inside the `/etc/systemd/system/multi-user.target.wants/`
directory:

```
sudo ln -s /root/localbit/localbit-daemon.service localbit-daemon.service
```

This script runs the `localbit` binary compiled above, which listens
to the ADC and the button and reports their status to the network, and
listens to the network for commands for changing the status of the
output and the LED.

If you want to turn it off, e.g. so that you can hack on it, run
`systemctl stop localbit-daemon`.


## The localbit network protocol

The protocol uses UDP.

The localbit program running on the cloudbit periodically sends the
following packet to the configured host on UDP port 2020:

 * 6 bytes of the cloudbit's MAC address.
 * 2 byte word in network byte order (big-endian) which is 0x0001 if
   the button is pressed and otherwise 0x0000.
 * 2 byte word in network byte order (big-endian) representing the
   input (0x0000 to 0xFFFF). The value is only significant to about
   about 10 bits, and even the last two of these "significant" bits
   fluctate a lot, so if 8 bit precision is enough, you can just treat
   the first of these two bytes as the value in the range 0 to 255 and
   ignore the second byte.

```
 00 01 02 03 04 05 06 07 08 09
+--+--+--+--+--+--+--+--+--+--+
|   MAC ADDRESS   |00| B| VAL |
+--+--+--+--+--+--+--+--+--+--+
  Button state --------'   |
  Input value -------------'     16 bit word, big-endian.

Button state:
MSB                   LSB
+--+--+--+--+--+--+--+--+
|  |  |  |  |  |  |  |SS|
+--+--+--+--+--+--+--+--+
  State --------------'          Whether the button is set.
                                 0x00 = no, 0x01 = pressed

```

The localbit program listens for UDP packets on port 2021. If they
start with 6 bytes that match the cloudbit's MAC address, then the
next byte is examined. This is the LED state byte. If this byte's 8th
bit is set (0x80), then then bits 1 through 3 of the LED state byte
are interpreted as the red, green, and blue channels for the LED
respectively. The byte after this is the output state byte. If the
high bit is set, then the next two bytes are interpreted as the 16 bit
value in network byte order (big-endian) to which to set the output
(0x0000 to 0xFFFF).

```
 00 01 02 03 04 05 06 07 08 09
+--+--+--+--+--+--+--+--+--+--+
|   MAC ADDRESS   |LL|OO| VAL |
+--+--+--+--+--+--+--+--+--+--+
  LED state -------'  |    |
  Output state -------'    |
  Output value ------------'     16 bit word, big-endian.

LED state:
MSB                   LSB
+--+--+--+--+--+--+--+--+
|CC|  |  |  |  |RR|GG|BB|
+--+--+--+--+--+--+--+--+
 '---- Control  |  |  |          Whether to change the LED.
  Red ----------'  |  |          If CC is set, new red channel state.
  Green -----------'  |          If CC is set, new green channel state.
  Blue ---------------'          If CC is set, new blue channel state.

Output state:
MSB                   LSB
+--+--+--+--+--+--+--+--+
|CC|  |  |  |  |  |  |  |
+--+--+--+--+--+--+--+--+
 '---- Control                   Whether to change the output value.
```

The rate of messages being sent from the cloudbit increases if the
input's value fluctuates or if the button is pressed, and decreases if
the input's value remains steady. This is implemented by inserting a
delay in the program loop. If the input's value is seen to fluctate
dramatically, the delay is set to its minimum value immediately; if
the rate of change is slow, then the delay is decreased by 10ms; if
the value barely changes at all, then the rate is increased by 10ms up
to the maximum delay.

The fastest rate at which messages are being sent (e.g. while the
button is being held down), is 100Hz (one message every 10ms). The
slowest rate is about 0.5Hz (a delay of 2010ms).

A new message is always sent immediately when one is received, without
affecting the steady state rate, so the value may be polled by sending
a message with the two control bits set to zero (i.e. a ten byte
message consisting of the six MAC address bytes followed by four zero
bytes). The minimum delay is still enforced; at most one packet is
consumed every 10ms.

This variable rate is intended to keep the load on the cloudbit CPU to
a reasonable level so that the cloudbit can run continually with
minimal risk of damage. Be wary of sending polling messages at a high
rate continually since doing so defeats this mitigation.


# Notes on the w20 cloudbit

The remainder of this file consists of notes I made while
reverse-engineering the cloudbit.

## Hardware

According to `/proc/cpuinfo` and `/proc/meminfo`, the cloudbit has a
cARM926EJ-S rev 5 (v5l) processor (113.04 BogoMIPS), 59844 kB of
RAM, and a ~2.6GB SD card.

The schematic for the cloudbit is available [on
GitHub](https://github.com/littlebits/w20-cloud/raw/master/hardware/LB_BIT_w20_cloudV1-(3_3)OHW/w20-cloudV1-(3_3)OHW.pdf).

The hardware is controlled from code by manipulating memory locations,
as described below. The hardware is physically connected to pins on
the chip on the cloudbit. It's not clear to me how the pins map to
memory locations.

The cloudbit ships with some proprietary Littlebits programs. To
determine their behaviour, a program such as
[retdec](https://retdec.com/) is invaluable.


### The LED

The program `/usr/local/lb/LEDcolor/bin/LEDcolor.d`, which is run when
the cloudbit boots, opens a Unix domain socket
(`/var/lb/SET_COLOR_socket`), and executes commands written into that
socket to change the colour of the LED.

The program `/usr/local/lb/LEDcolor/bin/setColor` writes commands
given on its command line to that socket.

The commands are colours, e.g. specifically `red`, `green`, `blue`,
`yellow`, `teal`, `purple` (or `violet`), and `white`, the command
`off` which turns off the LED entirely, the two commands `blink` and
`hold` which stop and start a periodic timer that switches between the
current colour and `off`, and the command `clownbarf` which I'll leave
as a surprise. The command `off` has no effect if a `blink` is in
effect, and `blink` has no effect if `off` was the last colour
triggered. Setting a colour seems to reset the blink timer, with the
LED enabled.

The 2KB page starting at 0x80018000 is used by the `LEDcontrol.d`
program to control the LED. This will be called _gpioPage_ below.

When the program starts, it sets the following memory locations to the
following values, in the following order:

| Address             | Value      |
|---------------------|------------|
| _gpioPage_ + 0x0114 | 0xF0000000 |
| _gpioPage_ + 0x0134 | 0x03000000 |
| _gpioPage_ + 0x0704 | 0x40000000 |
| _gpioPage_ + 0x0714 | 0x10000000 |

I've no idea what these values do.

The LED is controlled by three lines, red, green, and blue, which can
be turned on or off. To turn one of the lines on, you send the value
to the address to turn on, and to turn it off, you send the value to
the address to turn it off.

| Color | Address to turn on  | Address to turn off | Value      |
|-------|---------------------|---------------------|------------|
| Red   | _gpioPage_ + 0x0508 | _gpioPage_ + 0x0504 | 0x80000000 |
| Green | _gpioPage_ + 0x0508 | _gpioPage_ + 0x0504 | 0x40000000 |
| Blue  | _gpioPage_ + 0x0518 | _gpioPage_ + 0x0514 | 0x10000000 |

These channels correspond to pins 128, 127, and 91 respectively on the
big box on the schematic.


#### Areas for further research

I haven't tested to see if you can read the addresses listed above. I
haven't tested what happens if you try to read or write nearby
addresses, or set values other than those listed above (except I did
once try to set one of those addresses to 0xFFFFFFFF and the wifi
dropped until I rebooted the device, though the device itself was
still executing code, which I could tell because the LED was still
blinking).


### Analog-to-digital convertor (input)

As with the LED, the ADC is controlled by a program
`/usr/local/lb/ADC/bin/ADC.d`, which is run when the cloudbit boots,
and which opens a Unix domain socket (`/var/lb/ADC_socket`). When a
message is written to the socket, it outputs the current value of the
input to the socket.
 
The program `/usr/local/lb/ADC/bin/getADC` interacts with this socket.
You give it an arbitrary argument, such as `-1`, and it outputs two
lines each containing a number. The first is a number in the range 0
to 255 which corresponds to the input being in the range 0.0V to 5.0V.
The second is a number if the range 0 to 15 and I've no idea what it
corresponds to, except that when the input is off it's 0, when the
input is on full it's 15, and when the input is between those it
varies wildly between those two numbers (seemingly not with any
correlation to the input value).

The 2KB page starting at 0x80050000 is used by the `ADC.d` program to
read the input. This will be called _adcPage_ below.

When the program starts, it sets the following memory locations to the
following values, in the following order (note that the values are not
in ascending order):

| Address            | Value      |
|--------------------|------------|
| _adcPage_ + 0x0008 | 0x40000000 |
| _adcPage_ + 0x0004 | 0x00000001 |
| _adcPage_ + 0x0028 | 0x01000000 |
| _adcPage_ + 0x0014 | 0x00010000 |
| _adcPage_ + 0x0034 | 0x00000001 |
| _adcPage_ + 0x0024 | 0x01000000 |
| _adcPage_ + 0x0144 | 0x00000000 |

I've no idea what these values do. It's worth noting that there's also
code in `ADC.d` to do something with memory at address 0x80068000,
though it doesn't appear to actually be used.

> Updating the system memory with the values above is not sufficient
> to make the ADC work. If you set the values above then read the ADC
> values as described below, the program will hang. However, if you
> run `ADC.d`, then kill it, then read the value as described below,
> it works fine. I have no explanation.

To read the ADC value, it sets the value at _adcPage_ + 0x0004 to
0x00000001, waits until the high bit of the 32 bit word at _adcPage_ +
0x0050 changes value, then sets the value at _adcPage_ + 0x0018 to
0x00000001. The value of the ADC is the low 31 bits of the 32 bit word
at _adcPage_ + 0x0050. (Actually, more like the low 11 bits.)

The value appears to be a number in the range ~200&plusmn;1 (when the
input is at 0V) to ~1700&plusmn;10 (when the input is at a nominal 5V,
though it sometimes dips as far as ~1660 or so even when the input is
supposedly fully on).

Pins 115 and 108 on the schematic, labeled LINE1_INL and LRADC0
respectively, are connected to SIGIN1, which corresponds to the input
side bitSnap, which is the input to the ADC.


#### Areas for further research

I haven't determined what `ADC.d` does that needs to be done to
actually read the ADC.


### Digital-to-analog convertor (output)

The pattern repeats itself with the DAC, but with a bit of a twist.

When the cloudbit boots, it first runs
`/usr/local/lb/DAC/bin/DAC_init`, which does a bunch of memory
manipulation that I haven't tried to understand.

There is a socket-listening program, `/usr/local/lb/DAC/bin/DAC.d`,
which is run when the cloudbit boots, after `DAC_init`. It opens a
Unix domain socket (`/var/lb/DAC_socket`) and listens to it. When a
message is written to the socket, it tries to parse it as up to four
hexadecimal digits (0-9, a-f; case-insensitive), and then sets the
output of the cloudbit to a voltage by doing a linear mapping from the
range 0x0000 to 0xFFFF to the range 0.0V to 5.0V. If the message is
not one to four hexadecimal characters, it is ignored.

The program appears to try to log errors to `/var/lb/dacerrorlog`
though I haven't ever seen that file be non-empty.
 
The program `/usr/local/lb/ADC/bin/setDAC` interacts with the
aforementioned socket by just passing whatever argument it is given to
the socket (unless it's `--help`, in which case it outputs a mostly
useless help message instead).

The 2KB page starting at 0x80050000 is used by the `DAC.d` program to
set the output. This will be called _dacPage_ below. (The `DAC_init`
program also uses that page as well as two other pages.)

There are two addresses that are used by `DAC.d` as far as I can tell.
The first is _dacPage_ + 0x00F0, which is used to set the value. The
range 0x8000 to 0xFFFF seems to represent voltages in the range 0.0V
to 2.5V, and the range 0x0000 to 0x7FFF seems to represents voltages
in the range 2.5V to 5.0V. Thus, if _value_ is in the range 0x0000 to
0xFFFF, set the memory location at _dacPage_ + 0x00F0 to (_value_ +
0x8000) % 0x10000.

The second address used by `DAC.d` is _dacPage_ + 0x0040, which is
used as a signalling mechanism; specifically it appears that the
second bit at that address toggles when the DAC is done dealing with
the last value that was set.

In practice, setting the value at _dacPage_ + 0x00F0 doesn't take
until you've actually set it three or four times, so `DAC.d` tries to
set it up to 20 times in a row each time it is supposed to update the
DAC.

Pin 113 on the schematic, labeled HPL, corresponds to the DAC's
output, the output side bitSnap.


#### Areas for further research

I haven't determined what `DAC_init` does.


### The Button

The button is managed by `/usr/local/lb/Button/bin/button.d`. It uses
the socket `/var/lb/BUTTON_socket`. If the button is pressed for long
enough, `button.d` itself will run
`/usr/local/lb/comm-util/signal_netmon.sh` directly.

The `/usr/local/lb/Button/bin/getButton` program communicates via the
socket to report the state of the button.

The `button.d` program operates in a manner similar to the other
programs, reading memory locations to find the state of the hardware.
Specifically, it uses the same 2KB page starting at 0x80018000 the
`LEDcontrol.d` program uses to control the LED. This will again be
called _gpioPage_ below.

When the program starts, it sets the following memory locations to the
following values, in the following order:

| Address             | Value      |
|---------------------|------------|
| _gpioPage_ + 0x0124 | 0x0000C000 |
| _gpioPage_ + 0x0718 | 0x00000080 |

The actual status of the button is then available as bit 8 of the 32
bit word at _gpioPage_ + 0x0610. (The other bytes seem unrelated to
the button, but were not set to zero when I was testing this.)

The button is wired to pin 9 on the schematic, labeled LCD_D07 and
TACTSWITCH.


## Original software

The cloudbit uses a Linux distribution with `systemd` as the init, and
`pacman` as the package manager. I presume it's an archlinux variant.

`uname -a` reports the following:

```
Linux noctilucent 3.7.2-8-littleARCH #1 PREEMPT Fri May 16 11:39:57 EDT 2014 armv5tejl GNU/Linux
```

Some interesting unique aspects of the configuration:

* `/var/lb/mac` contains the MAC address of the cloudbit. It is
  created by `/usr/local/lb/bit-util/onBoot.sh` which runs early on
  boot.

* `/etc/wpa_supplicant/cloudbit.conf` contains the network
  configuration last provided by the user when commissioning the
  device (including the password unencrypted).

* There's a pair of shell scripts that set the CPU speed:
  `/usr/local/lb/bit-util/hiPowerSet.sh` and
  `/usr/local/lb/bit-util/lowPowerSet.sh`.
