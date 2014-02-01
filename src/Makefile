CFLAGS = -g -Wall -Wextra -MMD \
-I /opt/vc/include \
-I /opt/vc/include/interface/vcos/pthreads \
-I /opt/vc/include/interface/vmcs_host/linux \
-I /usr/include/glib-2.0 \
-I /usr/lib/arm-linux-gnueabihf/glib-2.0/include \
-I /usr/include/freetype2

mpddisplay: main.o mpd_intf.o display_intf.o vg_font.o
	gcc -o mpddisplay main.o mpd_intf.o display_intf.o vg_font.o \
-L /opt/vc/lib -lGLESv2 -lfreetype -lglib-2.0

clean:
	rm main.o mpd_intf.o display_intf.o vg_font.o mpddisplay

-include main.d mpd_intf.d display_intf.d vg_font.d
