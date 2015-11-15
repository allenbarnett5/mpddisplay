mpddisplay
==========

Display song metadata on a display attached to a Raspberry Pi

For the C version, cd into the src directory and type make. You will
have to have installed a massive number of -dev dependencies including:

* libmpdclient-dev
* libglib-2.0-dev
* libgdk-pixbuf2.0-dev
* libraspberrypi0
* libfreetype6-dev
* libsqlite3-dev
* libpango1.0-dev
* liblog4c-dev
* ... more to come ...

Also expects the setuid "gpio" command from WiringPi to be installed
(if you have buttons, for example on a Pibrella).

(Still trying to get the hang of Git and Markdown.)
