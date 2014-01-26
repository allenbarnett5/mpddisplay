CFLAGS = -g -Wall -Wextra -MMD

mpddisplay: main.o mpd_intf.o
	gcc -o mpddisplay main.o mpd_intf.o -lfirestring

clean:
	rm main.o mpd_intf.o mpddisplay

-include main.d mpd_intf.d
