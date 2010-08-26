## Настройки компилятора
#CC=i686-mingw32-gcc

ifndef CC
CC = gcc
endif

#ifndef CFLAGS
CFLAGS = -O0 -pipe -shared -fPIC -DPIC -g -ggdb -std=gnu99 -pedantic
#CFLAGS = -Os -pipe -shared -fPIC -DPIC  -std=gnu99
#endif

PURPLE_CFLAGS = $(CFLAGS)
PURPLE_CFLAGS += $(shell pkg-config --cflags purple)
#PURPLE_CFLAGS += $(shell pkg-config --libs glib-2.0)

#INCLUDE_PATHS := -I$(PIDGIN_TREE_TOP)/../win32-dev/w32api/include
#LIB_PATHS := -L$(PIDGIN_TREE_TOP)/../win32-dev/w32api/lib

#PURPLE_CFLAGS += $(INCLUDE_PATHS) 
#PURPLE_CFLAGS += $(LIB_PATHS) 

## Сборка
all:
	rm -f *.so
	${CC} ${PURPLE_CFLAGS} message.c cl.c package.c mrim.c -o mrim.so
#	strip -s mrim.so
install:
	install -Dm0644 mrim.so ${DESTDIR}/usr/lib/purple-2/mrim.so
	install -Dm0644 pixmaps/mrim16.png  ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/16/mrim.png
	install -Dm0644 pixmaps/mrim22.png  ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/22/mrim.png
	install -Dm0644 pixmaps/mrim48.png  ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/48/mrim.png
win32:
	rm -f *.dll
	${CC} ${PURPLE_CFLAGS} cl.c package.c mrim.c -o mrim.dll
	strip -s mrim.dll
uninstall:
	rm -f ${DESTDIR}/usr/lib/purple-2/mrim.so
	rm -f ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/16/mrim.png
	rm -f ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/22/mrim.png
	rm -f ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/48/mrim.png
clean:
	rm -rf *.o *.c~ *.h~ *.so *.la *.libs *.dll
