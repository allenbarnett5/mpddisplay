OBJCOPY = objcopy
BFDNAME = elf32-littlearm
BFDARCH = arm
CFLAGS = -g -Wall -Wextra -MMD \
-I /opt/vc/include \
-I /opt/vc/include/interface/vcos/pthreads \
-I /opt/vc/include/interface/vmcs_host/linux \
-I /usr/include/glib-2.0 \
-I /usr/lib/arm-linux-gnueabihf/glib-2.0/include \
-I /usr/include/pango-1.0 \
-I /usr/include/freetype2 \
-I /usr/include/ImageMagick

mpddisplay: main.o mpd_intf.o display_intf.o vg_font.o text_widget.o \
magick_image.o cover_image.o no_cover.o image_widget.o
	gcc -o mpddisplay main.o mpd_intf.o display_intf.o vg_font.o \
text_widget.o magick_image.o cover_image.o no_cover.o image_widget.o \
-L /opt/vc/lib -lGLESv2  -lpangoft2-1.0 -lpango-1.0 -lfreetype -lglib-2.0 \
-lMagickWand

no_cover.o: no_cover.png
	$(OBJCOPY) --input-target=binary --output-target=$(BFDNAME) \
--binary-architecture=$(BFDARCH) no_cover.png no_cover.o

clean:
	rm -f *.o mpddisplay

-include main.d mpd_intf.d display_intf.d vg_font.d text_widget.d \
magick_image.d cover_image.d image_widget.d
