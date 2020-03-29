# WLRoots DLNA Screen Cast

POC with casting of WLRoots output to DLNA device (screen) using screencopy protocol.

## How to use

1. Make sure you using wlroots-based window manager
2. Run `make` - it will compile the binary or show you what you've missed in dependencies
3. Modify bottom of the dlna-screencast.py to put your DLNA device address and workstation serve addres
4. Run `make run` command to start the screenshare

## Tests

It's a POC, but it could provide ~3-5 sec delay (depends on the mandatory DLNA buffering).

Tested with MiraCast device, connected with WiFi N router and SwayWM 1.4.
