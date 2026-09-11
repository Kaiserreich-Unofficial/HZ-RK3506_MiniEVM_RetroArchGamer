#!/bin/sh
# Disabled: conflicts with S40usbserial (ttyGS0 console) - both bind the same UDC.
case "$1" in
	start|stop|restart) exit 0 ;;
esac
exit 0
