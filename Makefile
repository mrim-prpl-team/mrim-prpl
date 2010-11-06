## Настройки компилятора

ifndef CC
CC = gcc
endif

ifndef LIBDIR
LIBDIR="lib"
endif

#ifndef CFLAGS
#CFLAGS = -O1 -pipe -shared -fPIC -DPIC -g -ggdb -std=gnu99 -pedantic
CFLAGS = -Os -pipe -shared -fPIC -DPIC  -std=gnu99
#endif

PURPLE_CFLAGS = $(CFLAGS)
PURPLE_CFLAGS += $(shell pkg-config --cflags purple)
#PURPLE_CFLAGS += $(shell pkg-config --libs glib-2.0)

#INCLUDE_PATHS := -I$(PIDGIN_TREE_TOP)/../win32-dev/w32api/include
#LIB_PATHS := -L$(PIDGIN_TREE_TOP)/../win32-dev/w32api/lib

#PURPLE_CFLAGS += $(INCLUDE_PATHS) 
#PURPLE_CFLAGS += $(LIB_PATHS) 

## Сборка
# Для gentoo: make compile
all:
	make compile
	strip -s mrim.so
compile:
	rm -fv *.so
	${CC} ${PURPLE_CFLAGS} message.c cl.c package.c mrim.c -o mrim.so
debug:
	make compile
install:
	install -Dm0644 mrim.so ${DESTDIR}/usr/${LIBDIR}/purple-2/mrim.so
	install -Dm0644 pixmaps/mrim16.png  ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/16/mrim.png
	install -Dm0644 pixmaps/mrim22.png  ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/22/mrim.png
	install -Dm0644 pixmaps/mrim48.png  ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/48/mrim.png
win32:
	rm -fv *.dll
	${CC} ${PURPLE_CFLAGS} message.c cl.c package.c mrim.c -o mrim.dll
	strip -s mrim.dll
uninstall:
	rm -fv ${DESTDIR}/usr/${LIBDIR}/purple-2/mrim.so
	rm -fv ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/16/mrim.png
	rm -fv ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/22/mrim.png
	rm -fv ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/48/mrim.png
clean:
	rm -rfv *.o *.c~ *.h~ *.so *.la *.libs *.dll
