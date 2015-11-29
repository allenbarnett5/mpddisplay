OBJCOPY = objcopy
BFDNAME = elf32-littlearm
BFDARCH = arm
CFLAGS = -g -Wall -Wextra -Wstrict-prototypes -MMD \
-I /opt/vc/include \
-I /opt/vc/include/interface/vcos/pthreads \
-I /opt/vc/include/interface/vmcs_host/linux \
-I /usr/include/glib-2.0 \
-I /usr/lib/arm-linux-gnueabihf/glib-2.0/include \
-I /usr/include/pango-1.0 \
-I /usr/include/freetype2 \
-I /usr/include/gdk-pixbuf-2.0

mpddisplay: main.o mpd_intf.o display_intf.o text_widget.o \
image_intf.o cover_image.o no_cover.o image_widget.o pattern.o log_intf.o \
empty_cover.o
	gcc -o mpddisplay main.o mpd_intf.o display_intf.o \
text_widget.o image_intf.o cover_image.o no_cover.o image_widget.o \
pattern.o log_intf.o empty_cover.o \
-L /opt/vc/lib -lGLESv2 -lEGL -lbcm_host \
-lpangoft2-1.0 -lpango-1.0 -lfreetype \
-lgio-2.0 -lgdk_pixbuf-2.0 -lglib-2.0 -lgobject-2.0 \
-lsqlite3 -llog4c -lmpdclient

no_cover.o: no_cover.png
	$(OBJCOPY) --input-target=binary --output-target=$(BFDNAME) \
--binary-architecture=$(BFDARCH) no_cover.png no_cover.o

empty_cover.o: empty_cover.png
	$(OBJCOPY) --input-target=binary --output-target=$(BFDNAME) \
--binary-architecture=$(BFDARCH) empty_cover.png empty_cover.o

pattern.o: pattern.png
	$(OBJCOPY) --input-target=binary --output-target=$(BFDNAME) \
--binary-architecture=$(BFDARCH) pattern.png pattern.o

clean:
	rm -f *.o mpddisplay

extraclean: clean
	rm -f *.d
-include main.d mpd_intf.d display_intf.d text_widget.d \
image_intf.d cover_image.d image_widget.d log_intf.d
