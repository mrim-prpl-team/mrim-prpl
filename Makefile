## Настройки компилятора
ifndef CC
CC = gcc
endif

ifndef LIBDIR
LIBDIR=lib
endif

ifndef PODIR
PODIR=po
endif

ifndef CFLAGS
CFLAGS = -Os -pipe
endif

ifdef CFLAGS
CFLAGS += -shared -fPIC -DPIC  -std=gnu99
endif

DEBUG_CFLAGS = -Wall -Wextra -Wconversion -Wsign-conversion -Winit-self -Wunreachable-code --pedantic  -Wstrict-aliasing

PURPLE_CFLAGS = $(shell pkg-config --cflags purple)
#PURPLE_CFLAGS += $(shell pkg-config --libs glib-2.0)
GTK_CFLAGS = $(shell pkg-config --libs --cflags gtk+-2.0)

all:compile i18n
	strip -s libmrim.so
compile:
	${CC} ${CFLAGS} ${PURPLE_CFLAGS} ${GTK_CFLAGS} ${LDFLAGS} message.c cl.c package.c mrim.c filetransfer.c mrim-util.c -o libmrim.so
debug:
	make clean
	${CC} ${CFLAGS} ${DEBUG_CFLAGS} ${PURPLE_CFLAGS} ${GTK_CFLAGS} ${LDFLAGS} message.c cl.c package.c mrim.c filetransfer.c mrim-util.c -o libmrim.so
	make i18n
i18n:
	msgfmt ${PODIR}/mrim-prpl-ru_RU.po --output-file=${PODIR}/mrim-prpl-ru_RU.mo
	msgfmt ${PODIR}/mrim-prpl-be_BY.po --output-file=${PODIR}/mrim-prpl-be_BY.mo
	msgfmt ${PODIR}/mrim-prpl-uk.po --output-file=${PODIR}/mrim-prpl-uk.mo
	msgfmt ${PODIR}/mrim-prpl-pl_PL.po --output-file=${PODIR}/mrim-prpl-pl_PL.mo
	msgfmt ${PODIR}/mrim-prpl-es_ES.po --output-file=${PODIR}/mrim-prpl-es_ES.mo
install:
	install -Dm0755 libmrim.so ${DESTDIR}/usr/${LIBDIR}/purple-2/libmrim.so
	install -Dm0644 pixmaps/mrim16.png  ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/16/mrim.png
	install -Dm0644 pixmaps/mrim22.png  ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/22/mrim.png
	install -Dm0644 pixmaps/mrim48.png  ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/48/mrim.png
	install -Dm0644 po/mrim-prpl-ru_RU.mo ${DESTDIR}/usr/share/locale/ru/LC_MESSAGES/mrim-prpl.mo
	install -Dm0644 po/mrim-prpl-be_BY.mo ${DESTDIR}/usr/share/locale/be_BY/LC_MESSAGES/mrim-prpl.mo
	install -Dm0644 po/mrim-prpl-uk.mo    ${DESTDIR}/usr/share/locale/uk/LC_MESSAGES/mrim-prpl.mo
	install -Dm0644 po/mrim-prpl-pl_PL.mo    ${DESTDIR}/usr/share/locale/pl_PL/LC_MESSAGES/mrim-prpl.mo
	install -Dm0644 po/mrim-prpl-es_ES.mo    ${DESTDIR}/usr/share/locale/es_ES/LC_MESSAGES/mrim-prpl.mo
uninstall:uninstall-old
	rm -fv ${DESTDIR}/usr/${LIBDIR}/purple-2/libmrim.so
	rm -fv ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/16/mrim.png
	rm -fv ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/22/mrim.png
	rm -fv ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/48/mrim.png
	rm -fv ${DESTDIR}/usr/share/locale/ru/LC_MESSAGES/mrim-prpl.mo
	rm -fv ${DESTDIR}/usr/share/locale/be_BY/LC_MESSAGES/mrim-prpl.mo
	rm -fv ${DESTDIR}/usr/share/locale/uk/LC_MESSAGES/mrim-prpl.mo
uninstall-old:
	rm -fv ${DESTDIR}/usr/${LIBDIR}/purple-2/mrim.so
clean:
	rm -rfv *.o *.c~ *.h~ *.so *.la *.libs *.dll ${PODIR}/*.mo
