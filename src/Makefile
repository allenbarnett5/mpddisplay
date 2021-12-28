OBJCOPY = objcopy
BFDNAME = elf32-littlearm
BFDARCH = arm
CFLAGS = -g -Wall -Wextra -Wstrict-prototypes -MMD \
-I /usr/include/libdrm \
-I/usr/include/glib-2.0 -I/usr/lib/arm-linux-gnueabihf/glib-2.0/include \
-I /usr/include/pango-1.0 \
-I /usr/include/freetype2 \
-I /usr/include/gdk-pixbuf-2.0

mpddisplay: main.o log_intf.o mpd_intf.o cover_image.o display_intf.o \
text_widget.o image_widget.o thermometer_widget.o emblem_widget.o \
image_intf.o no_cover.o empty_cover.o pattern.o ogles.o
	gcc -o $@ $^ -g \
-llog4c -lmpdclient -lsqlite3 \
-lgdk_pixbuf-2.0 -lgobject-2.0 -lgio-2.0 -lglib-2.0 \
-lpango-1.0 -lpangoft2-1.0 -lfreetype \
-ldrm -lgbm -lEGL -lGL -lm

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
-include *.d
