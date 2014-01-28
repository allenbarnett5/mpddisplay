CFLAGS = -g -Wall -Wextra -MMD \
-I /opt/vc/include \
-I /opt/vc/include/interface/vcos/pthreads \
-I/opt/vc/include/interface/vmcs_host/linux

mpddisplay: main.o mpd_intf.o display_intf.o
	gcc -o mpddisplay main.o mpd_intf.o display_intf.o \
-L /opt/vc/lib -lGLESv2 -lfirestring

clean:
	rm main.o mpd_intf.o display_intf.o mpddisplay

-include main.d mpd_intf.d display_intf.d
