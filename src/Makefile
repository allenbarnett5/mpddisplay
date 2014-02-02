CFLAGS = -g -Wall -Wextra -MMD \
-I /opt/vc/include \
-I /opt/vc/include/interface/vcos/pthreads \
-I /opt/vc/include/interface/vmcs_host/linux \
-I /usr/include/glib-2.0 \
-I /usr/lib/arm-linux-gnueabihf/glib-2.0/include \
-I /usr/include/pango-1.0 \
-I /usr/include/freetype2

mpddisplay: main.o mpd_intf.o display_intf.o vg_font.o text_widget.o
	gcc -o mpddisplay main.o mpd_intf.o display_intf.o vg_font.o text_widget.o \
-L /opt/vc/lib -lGLESv2  -lpangoft2-1.0 -lpango-1.0 -lfreetype -lglib-2.0

clean:
	rm -f *.o mpddisplay

-include main.d mpd_intf.d display_intf.d vg_font.d text_widget.d
