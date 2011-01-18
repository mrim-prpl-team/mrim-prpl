## Настройки компилятора

ifndef CC
CC = gcc
endif

ifndef LIBDIR
LIBDIR="lib"
endif

ifndef PODIR
PODIR=po
endif

# WTF DESTDIR is not used?

#ifndef CFLAGS
#CFLAGS = -O1 -pipe -shared -fPIC -DPIC -g -ggdb -std=gnu99 -pedantic
CFLAGS = -Os -pipe -shared -fPIC -DPIC  -std=gnu99
#endif

PURPLE_CFLAGS = $(CFLAGS)
PURPLE_CFLAGS += $(shell pkg-config --cflags purple)
#PURPLE_CFLAGS += $(shell pkg-config --libs glib-2.0)

all:
	make compile
	strip -s mrim.so
	make i18n
compile:
	${CC} ${PURPLE_CFLAGS} message.c cl.c package.c mrim.c filetransfer.c -o mrim.so
debug:
	make clean
	make compile
	make i18n
i18n:
	msgfmt ${PODIR}/mrim-prpl-ru_RU.po --output-file=${PODIR}/mrim-prpl-ru_RU.mo
	msgfmt ${PODIR}/mrim-prpl-uk.po --output-file=${PODIR}/mrim-prpl-uk.mo
install:
	install -Dm0644 mrim.so ${DESTDIR}/usr/${LIBDIR}/purple-2/mrim.so
	install -Dm0644 pixmaps/mrim16.png  ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/16/mrim.png
	install -Dm0644 pixmaps/mrim22.png  ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/22/mrim.png
	install -Dm0644 pixmaps/mrim48.png  ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/48/mrim.png
	install -Dm0644 po/mrim-prpl-ru_RU.mo ${DESTDIR}/usr/share/locale/ru/LC_MESSAGES/mrim-prpl.mo
	install -Dm0644 po/mrim-prpl-uk.mo ${DESTDIR}/usr/share/locale/uk/LC_MESSAGES/mrim-prpl.mo
uninstall:
	rm -fv ${DESTDIR}/usr/${LIBDIR}/purple-2/mrim.so
	rm -fv ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/16/mrim.png
	rm -fv ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/22/mrim.png
	rm -fv ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/48/mrim.png
	rm -fv ${DESTDIR}/usr/share/locale/ru/LC_MESSAGES/mrim-prpl.mo
clean:
	rm -rfv *.o *.c~ *.h~ *.so *.la *.libs *.dll ${PODIR}/*.mo
