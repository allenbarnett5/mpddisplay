mpddisplay
==========

Display song metadata on TV attached to Raspberry Pi

For the C version, cd into the src directory and type make. You will
have to have installed a massive number of -dev dependencies including:

* libmpdclient-dev
* libglib-2.0-dev
* libraspberrypi0
* libfreetype6-dev
* libsqlite3-dev
* libpango1.0-dev
* libmagickwand-dev -> leads to a lot shared libraries, including X's, sigh.
* liblog4c-dev
* ... more to come ...

Also expects the setuid "gpio" command from WiringPi to be installed
(if you have buttons, for example on a Pibrella).

(Still trying to get the hang of Git and Markdown.)
